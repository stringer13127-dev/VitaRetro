#include "source_pairing.hpp"
#include "vendor/qrcodegen.h"
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/rng.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/sysmodule.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* kSources = "ux0:/data/VitaRetro/sources.txt";
static const int kPort = 28781;
static VrSource sources[5];
static char pairUrl[100], pairPath[42], pairStatus[96];
static uint8_t qr[qrcodegen_BUFFER_LEN_MAX];
static uint8_t qrTemp[qrcodegen_BUFFER_LEN_MAX];
static uint8_t netMemory[1024 * 1024];
static int listener = -1, client = -1, activeSlot = -1;
static int clientFrames = 0;
static size_t requestLen = 0;
static char request[2048];
static bool netReady = false, received = false;

static bool parseUrl(const char* url, char* name) {
  if (!url) return false;
  size_t len = strlen(url);
  if (len < 10 || len >= sizeof(sources[0].url)) return false;
  const char* host = nullptr;
  if (strncmp(url, "https://", 8) == 0) host = url + 8;
  if (strncmp(url, "http://", 7) == 0) host = url + 7;
  if (!host || !*host) return false;
  const char* end = host;
  while (*end && *end != '/' && *end != '?' && *end != '#') {
    unsigned char c = (unsigned char)*end;
    if (c == '@' || c == '\\' || c <= 32 || c >= 127) return false;
    ++end;
  }
  if (end == host || end - host > 253 || host[0] == '.' || end[-1] == '.') return false;
  for (const char* p = end; *p; ++p) {
    if ((unsigned char)*p <= 32 || (unsigned char)*p == 127 || *p == '\\') return false;
  }
  size_t n = (size_t)(end - host);
  if (n > 27) n = 27;
  memcpy(name, host, n);
  name[n] = 0;
  return true;
}

const VrSource& vrSource(int slot) {
  return sources[slot >= 0 && slot < 5 ? slot : 0];
}

void vrSourcesLoad() {
  memset(sources, 0, sizeof(sources));
  SceUID f = sceIoOpen(kSources, SCE_O_RDONLY, 0);
  if (f < 0) f = sceIoOpen("ux0:/data/VitaRetro/sources.txt.bak", SCE_O_RDONLY, 0);
  if (f < 0) return;
  char data[sizeof(sources) + 16];
  int n = sceIoRead(f, data, sizeof(data) - 1);
  sceIoClose(f);
  if (n <= 0) return;
  data[n] = 0;
  char* line = data;
  for (int i = 0; i < 5 && line; ++i) {
    char* next = strchr(line, '\n');
    if (next) *next++ = 0;
    size_t l = strlen(line);
    if (l && line[l - 1] == '\r') line[l - 1] = 0;
    if (*line && parseUrl(line, sources[i].name)) strcpy(sources[i].url, line);
    line = next;
  }
}

bool vrSourceSet(int slot, const char* url) {
  if (slot < 0 || slot >= 5) return false;
  VrSource updated = {};
  if (!parseUrl(url, updated.name)) return false;
  strcpy(updated.url, url);
  sceIoMkdir("ux0:/data/VitaRetro", 0777);
  char tmp[128];
  snprintf(tmp, sizeof(tmp), "%s.tmp", kSources);
  SceUID f = sceIoOpen(tmp, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
  if (f < 0) return false;
  bool ok = true;
  for (int i = 0; i < 5; ++i) {
    const char* value = i == slot ? updated.url : sources[i].url;
    size_t len = strlen(value);
    if ((len && sceIoWrite(f, value, len) != (int)len) || sceIoWrite(f, "\n", 1) != 1) {
      ok = false; break;
    }
  }
  sceIoClose(f);
  if (ok) {
    char backup[128];
    snprintf(backup, sizeof(backup), "%s.bak", kSources);
    sceIoRemove(backup);
    bool hadOld = sceIoRename(kSources, backup) >= 0;
    ok = sceIoRename(tmp, kSources) >= 0;
    if (ok) sceIoRemove(backup);
    else if (hadOld) sceIoRename(backup, kSources);
  }
  if (!ok) { sceIoRemove(tmp); return false; }
  sources[slot] = updated;
  return true;
}

static void closeClient() {
  if (client >= 0) sceNetSocketClose(client);
  client = -1;
  clientFrames = 0;
  requestLen = 0;
}

void vrPairStop() {
  closeClient();
  if (listener >= 0) sceNetSocketClose(listener);
  listener = -1;
  activeSlot = -1;
  pairUrl[0] = 0;
  if (netReady) {
    sceNetCtlTerm();
    sceNetTerm();
    sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
    netReady = false;
  }
}

bool vrPairStart(int slot) {
  vrPairStop();
  received = false;
  snprintf(pairStatus, sizeof(pairStatus), "WIFI INDISPONIBLE");
  if (slot < 0 || slot >= 5) return false;
  if (sceSysmoduleLoadModule(SCE_SYSMODULE_NET) < 0) return false;
  SceNetInitParam init = {netMemory, sizeof(netMemory), 0};
  if (sceNetInit(&init) < 0) { sceSysmoduleUnloadModule(SCE_SYSMODULE_NET); return false; }
  netReady = true;
  if (sceNetCtlInit() < 0) { vrPairStop(); return false; }
  SceNetCtlInfo info;
  memset(&info, 0, sizeof(info));
  if (sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_IP_ADDRESS, &info) < 0 ||
      !info.ip_address[0] || strcmp(info.ip_address, "0.0.0.0") == 0) {
    vrPairStop(); return false;
  }
  uint8_t nonce[16];
  if (sceKernelGetRandomNumber(nonce, sizeof(nonce)) < 0) { vrPairStop(); return false; }
  strcpy(pairPath, "/");
  for (int i = 0; i < 16; ++i) snprintf(pairPath + 1 + 2 * i, 3, "%02x", nonce[i]);
  snprintf(pairUrl, sizeof(pairUrl), "http://%s:%d%s", info.ip_address, kPort, pairPath);
  if (!qrcodegen_encodeText(pairUrl, qrTemp, qr, qrcodegen_Ecc_MEDIUM,
                            qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX,
                            qrcodegen_Mask_AUTO, true)) { vrPairStop(); return false; }

  listener = sceNetSocket("VitaRetro pairing", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0);
  if (listener < 0) { vrPairStop(); return false; }
  int yes = 1;
  sceNetSetsockopt(listener, SCE_NET_SOL_SOCKET, SCE_NET_SO_REUSEADDR, &yes, sizeof(yes));
  SceNetSockaddrIn addr = {};
  addr.sin_len = sizeof(addr);
  addr.sin_family = SCE_NET_AF_INET;
  addr.sin_port = sceNetHtons(kPort);
  addr.sin_addr.s_addr = 0;
  if (sceNetBind(listener, (SceNetSockaddr*)&addr, sizeof(addr)) < 0 ||
      sceNetListen(listener, 1) < 0) { vrPairStop(); return false; }
  sceNetSetsockopt(listener, SCE_NET_SOL_SOCKET, SCE_NET_SO_NBIO, &yes, sizeof(yes));
  activeSlot = slot;
  snprintf(pairStatus, sizeof(pairStatus), "SCANNE LE QR AVEC TON TELEPHONE");
  return true;
}

const char* vrPairUrl() { return pairUrl; }
const char* vrPairStatus() { return pairStatus; }
const uint8_t* vrPairQr() { return activeSlot >= 0 ? qr : nullptr; }
bool vrPairReceived() { return received; }

static void sendReply(int code, const char* body) {
  if (client < 0) return;
  char header[256];
  size_t len = strlen(body);
  int h = snprintf(header, sizeof(header),
    "HTTP/1.1 %d %s\r\nContent-Type: text/html; charset=UTF-8\r\n"
    "Content-Length: %u\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n",
    code, code == 200 ? "OK" : "Bad Request", (unsigned)len);
  // Tiny single-response form; the socket remains nonblocking.
  if (h > 0) sceNetSend(client, header, h, 0);
  sceNetSend(client, body, len, 0);
  closeClient();
}

static int hex(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static bool decodeForm(const char* body, size_t length, char* url, size_t capacity) {
  if (length < 5 || strncmp(body, "url=", 4) != 0) return false;
  size_t out = 0;
  for (size_t i = 4; i < length; ++i) {
    if (body[i] == '&') break;
    unsigned char c = body[i];
    if (c == '+') c = ' ';
    else if (c == '%') {
      if (i + 2 >= length) return false;
      int a = hex(body[++i]), b = hex(body[++i]);
      if (a < 0 || b < 0) return false;
      c = (unsigned char)((a << 4) | b);
    }
    if (c == 0 || out + 1 >= capacity) return false;
    url[out++] = c;
  }
  url[out] = 0;
  return true;
}

static void handleRequest() {
  char* headers = strstr(request, "\r\n\r\n");
  if (!headers) return;
  size_t offset = (size_t)(headers + 4 - request);
  if (strncmp(request, "GET ", 4) == 0 &&
      strncmp(request + 4, pairPath, strlen(pairPath)) == 0 &&
      request[4 + strlen(pairPath)] == ' ') {
    sendReply(200, "<!doctype html><html lang=fr><meta name=viewport content='width=device-width,initial-scale=1'>"
      "<title>VitaRetro - Ajouter une source</title><body style='font:18px sans-serif;max-width:36em;margin:2em auto;padding:1em'>"
      "<h1>VitaRetro</h1><p>Colle l'adresse globale du site pour la source choisie sur ta Vita.</p>"
      "<form method=post><label>URL du site <input name=url type=url required maxlength=500 "
      "placeholder='https://exemple.org' style='display:block;width:100%;padding:.7em'></label>"
      "<button style='margin-top:1em;padding:1em' type=submit>Enregistrer sur la Vita</button></form>"
      "<p>Le telephone et la Vita doivent etre sur le meme Wi-Fi.</p></body></html>");
    return;
  }
  if (strncmp(request, "POST ", 5) == 0 &&
      strncmp(request + 5, pairPath, strlen(pairPath)) == 0 &&
      request[5 + strlen(pairPath)] == ' ') {
    const char* cl = strstr(request, "\r\nContent-Length:");
    if (!cl || cl > headers || offset > sizeof(request) || cl[17] == 0) {
      sendReply(400, "<p>Requete invalide.</p>"); return;
    }
    long bodyLength = strtol(cl + 17, nullptr, 10);
    if (bodyLength <= 0 || bodyLength > 1024) { sendReply(400, "<p>URL trop longue.</p>"); return; }
    if (requestLen - offset < (size_t)bodyLength) return;
    char url[512];
    if (!decodeForm(request + offset, (size_t)bodyLength, url, sizeof(url)) ||
        !vrSourceSet(activeSlot, url)) {
      sendReply(400, "<p>URL invalide ou impossible a enregistrer. Utilise http:// ou https://.</p>"); return;
    }
    received = true;
    snprintf(pairStatus, sizeof(pairStatus), "URL ENREGISTREE POUR SOURCE %d", activeSlot + 1);
    sendReply(200, "<meta name=viewport content='width=device-width,initial-scale=1'>"
      "<p>Source enregistree sur la Vita. Tu peux revenir a l'application.</p>");
    return;
  }
  sendReply(400, "<p>Page introuvable.</p>");
}

void vrPairPump() {
  if (activeSlot < 0) return;
  if (client < 0) {
    client = sceNetAccept(listener, nullptr, nullptr);
    if (client >= 0) {
      int yes = 1;
      sceNetSetsockopt(client, SCE_NET_SOL_SOCKET, SCE_NET_SO_NBIO, &yes, sizeof(yes));
      clientFrames = 0;
      requestLen = 0;
    }
    return;
  }
  if (++clientFrames > 300 || requestLen >= sizeof(request) - 1) { closeClient(); return; }
  int n = sceNetRecv(client, request + requestLen,
                     sizeof(request) - requestLen - 1, SCE_NET_MSG_DONTWAIT);
  if (n == 0) { closeClient(); return; }
  if (n > 0) {
    requestLen += n;
    request[requestLen] = 0;
    handleRequest();
  }
}
