#include "retroarch_manager.hpp"
#include "payload_manifest.hpp"
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/sysmodule.h>
#include <curl/curl.h>
#include <openssl/sha.h>
#include <zlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char* kPayload = "ux0:/data/VitaRetro/VitaRetro_0.1_DEV_RetroArch.vrp";
static const char* kPartial = "ux0:/data/VitaRetro/retroarch-download.part";
static const char* kInstalled = "ux0:/data/VitaRetro/.retroarch_deployed_1_22_2";
static unsigned char netMemory[4 * 1024 * 1024];

static void setError(char* error, size_t capacity, const char* message, int detail = 0) {
  if (error && capacity) snprintf(error, capacity, detail ? "%s %08X" : "%s", message, (unsigned)detail);
}

static bool readExact(SceUID fd, void* buffer, unsigned count) {
  char* p = (char*)buffer;
  while (count) {
    int n = sceIoRead(fd, p, count);
    if (n <= 0) return false;
    p += n;
    count -= n;
  }
  return true;
}

static unsigned read32(const uint8_t* p) {
  return (unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24;
}

static bool validRelativePath(const char* path) {
  if (!*path || *path == '/' || strlen(path) > 500) return false;
  const char* segment = path;
  for (const char* p = path; ; ++p) {
    unsigned char ch = (unsigned char)*p;
    if (ch == '\\' || ch == ':' || (ch < 32 && ch != 0)) return false;
    if (ch == '/' || !ch) {
      size_t n = (size_t)(p - segment);
      if (!n || (n == 1 && segment[0] == '.') ||
          (n == 2 && segment[0] == '.' && segment[1] == '.')) return false;
      if (!ch) return true;
      segment = p + 1;
    }
  }
}

static int makeDir(const char* path) {
  int result = sceIoMkdir(path, 0777);
  if (result >= 0) return 0;
  SceIoStat st;
  memset(&st, 0, sizeof(st));
  return sceIoGetstat(path, &st) >= 0 ? 0 : result;
}

static int makeParents(char* path, size_t prefix) {
  for (char* p = path + prefix; *p; ++p) {
    if (*p != '/') continue;
    *p = 0;
    int result = makeDir(path);
    *p = '/';
    if (result < 0) return result;
  }
  return 0;
}

static bool sha256Matches(const char* path, VrDeployProgress progress) {
  SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
  if (fd < 0) return false;
  SceIoStat st;
  memset(&st, 0, sizeof(st));
  if (sceIoGetstat(path, &st) < 0) { sceIoClose(fd); return false; }
  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  char buffer[32768];
  uint64_t current = 0;
  int n;
  while ((n = sceIoRead(fd, buffer, sizeof(buffer))) > 0) {
    SHA256_Update(&ctx, buffer, n);
    current += n;
    if (progress && (current % (1024 * 1024) < sizeof(buffer)))
      progress("VERIFICATION", current, st.st_size);
  }
  sceIoClose(fd);
  if (n < 0) return false;
  unsigned char digest[SHA256_DIGEST_LENGTH];
  SHA256_Final(digest, &ctx);
  char hex[SHA256_DIGEST_LENGTH * 2 + 1];
  for (unsigned i = 0; i < sizeof(digest); ++i)
    snprintf(hex + i * 2, 3, "%02x", digest[i]);
  if (progress) progress("VERIFICATION", current, st.st_size);
  return strcmp(hex, VR_PAYLOAD_SHA256) == 0;
}

struct DownloadContext { SceUID fd; VrDeployProgress progress; };

static size_t writeDownload(char* data, size_t size, size_t number, void* user) {
  DownloadContext* ctx = (DownloadContext*)user;
  if (number && size > SIZE_MAX / number) return 0;
  size_t length = size * number, sent = 0;
  while (sent < length) {
    int n = sceIoWrite(ctx->fd, data + sent, length - sent);
    if (n <= 0) return sent;
    sent += n;
  }
  return sent;
}

static int downloadProgress(void* user, curl_off_t total, curl_off_t current,
                            curl_off_t, curl_off_t) {
  DownloadContext* ctx = (DownloadContext*)user;
  if (ctx->progress) ctx->progress("TELECHARGEMENT", current, total);
  return 0;
}

static int downloadPayload(VrDeployProgress progress) {
  int result = sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
  if (result < 0) return result;
  SceNetInitParam init = {netMemory, sizeof(netMemory), 0};
  result = sceNetInit(&init);
  if (result < 0) { sceSysmoduleUnloadModule(SCE_SYSMODULE_NET); return result; }
  result = sceNetCtlInit();
  if (result < 0) { sceNetTerm(); sceSysmoduleUnloadModule(SCE_SYSMODULE_NET); return result; }

  result = -1;
  if (curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK) {
    SceUID fd = sceIoOpen(kPartial, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    if (fd >= 0) {
      DownloadContext context = {fd, progress};
      CURL* curl = curl_easy_init();
      if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, VR_PAYLOAD_URL);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "VitaRetro/0.1 DEV PS Vita");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 20L);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(curl, CURLOPT_CAINFO, "app0:/cacert.pem");
        curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
        curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeDownload);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, downloadProgress);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &context);
        CURLcode code = curl_easy_perform(curl);
        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        if (code == CURLE_OK && status == 200) result = 0;
        else result = -1000 - (int)code;
        curl_easy_cleanup(curl);
      }
      sceIoClose(fd);
    } else result = fd;
    curl_global_cleanup();
  }
  sceNetCtlTerm();
  sceNetTerm();
  sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
  if (result == 0) {
    sceIoRemove(kPayload);
    result = sceIoRename(kPartial, kPayload);
  }
  if (result < 0) sceIoRemove(kPartial);
  return result;
}

static int extractFile(SceUID archive, SceUID output, unsigned compressed,
                       unsigned expected, unsigned expectedCrc) {
  z_stream stream;
  memset(&stream, 0, sizeof(stream));
  if (inflateInit(&stream) != Z_OK) return -1;
  uint8_t input[16384], plain[16384];
  unsigned remaining = compressed, count = 0;
  uLong crc = crc32(0L, Z_NULL, 0);
  int result = -1;
  for (;;) {
    if (!stream.avail_in && remaining) {
      unsigned take = remaining < sizeof(input) ? remaining : sizeof(input);
      if (!readExact(archive, input, take)) break;
      stream.next_in = input;
      stream.avail_in = take;
      remaining -= take;
    }
    stream.next_out = plain;
    stream.avail_out = sizeof(plain);
    int zr = inflate(&stream, Z_NO_FLUSH);
    unsigned produced = sizeof(plain) - stream.avail_out;
    if (produced) {
      crc = crc32(crc, plain, produced);
      count += produced;
      int written = 0;
      bool writeFailed = false;
      while (written < (int)produced) {
        int n = sceIoWrite(output, plain + written, produced - written);
        if (n <= 0) { result = n < 0 ? n : -1; writeFailed = true; break; }
        written += n;
      }
      if (writeFailed) break;
    }
    if (zr == Z_STREAM_END) {
      if (!remaining && !stream.avail_in && count == expected && crc == expectedCrc)
        result = 0;
      break;
    }
    if (zr != Z_OK || (!remaining && !stream.avail_in && !produced)) break;
  }
  inflateEnd(&stream);
  return result;
}

static int extractPayload(VrDeployProgress progress) {
  SceUID archive = sceIoOpen(kPayload, SCE_O_RDONLY, 0);
  if (archive < 0) return archive;
  uint8_t header[14];
  if (!readExact(archive, header, 12) || memcmp(header, "VRPAY001", 8)) {
    sceIoClose(archive); return -1;
  }
  unsigned count = read32(header + 8);
  if (count < 100 || count > 100000) { sceIoClose(archive); return -1; }
  int result = makeDir("ux0:/app/VRET00001/retroarch");
  if (result >= 0) result = makeDir("ux0:/data/retroarch");
  if (result < 0) { sceIoClose(archive); return result; }
  if (progress) progress("DEPLOIEMENT", 0, count);
  for (unsigned i = 0; i < count; ++i) {
    if (!readExact(archive, header, sizeof(header))) { result = -1; break; }
    unsigned length = (unsigned)header[0] | (unsigned)header[1] << 8;
    unsigned packed = read32(header + 2), raw = read32(header + 6);
    unsigned checksum = read32(header + 10);
    char name[501], path[640], temporary[650];
    if (!length || length > 500 || raw > 512u * 1024u * 1024u ||
        !readExact(archive, name, length)) { result = -1; break; }
    name[length] = 0;
    if (!validRelativePath(name)) { result = -1; break; }
    const char* root = nullptr;
    const char* relative = nullptr;
    if (strncmp(name, "retroarch/", 10) == 0) {
      root = "ux0:/app/VRET00001/retroarch/"; relative = name + 10;
    } else if (strncmp(name, "data/", 5) == 0) {
      root = "ux0:/data/retroarch/"; relative = name + 5;
    }
    if (!root || !validRelativePath(relative)) { result = -1; break; }
    int n = snprintf(path, sizeof(path), "%s%s", root, relative);
    if (n < 0 || n >= (int)sizeof(path)) { result = -1; break; }
    result = makeParents(path, strlen(root));
    if (result < 0) break;
    if (vrFileExists(path)) {
      if (sceIoLseek(archive, packed, SCE_SEEK_CUR) < 0) { result = -1; break; }
    } else {
      n = snprintf(temporary, sizeof(temporary), "%s.vr-tmp", path);
      if (n < 0 || n >= (int)sizeof(temporary)) { result = -1; break; }
      SceUID output = sceIoOpen(temporary, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
      if (output < 0) { result = output; break; }
      result = extractFile(archive, output, packed, raw, checksum);
      sceIoClose(output);
      if (result == 0) result = sceIoRename(temporary, path);
      if (result < 0) { sceIoRemove(temporary); break; }
    }
    if (progress && (i % 16 == 0 || i + 1 == count))
      progress("DEPLOIEMENT", i + 1, count);
  }
  sceIoClose(archive);
  if (result < 0) return result;
  result = makeDir("ux0:/data/retroarch/system");
  if (result < 0) return result;
  if (!vrRetroArchPayloadPresent()) return -1;
  SceUID marker = sceIoOpen(kInstalled, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
  if (marker < 0) return marker;
  sceIoClose(marker);
  sceIoRemove(kPayload);  // Free the downloaded archive after a verified deployment.
  return 0;
}

int vrDeployRetroArch(VrDeployProgress progress, char* error, size_t errorSize) {
  if (error && errorSize) error[0] = 0;
  if (vrRetroArchPayloadPresent() && vrFileExists(kInstalled)) return 0;
  int result = makeDir("ux0:/data/VitaRetro");
  if (result < 0) { setError(error, errorSize, "STOCKAGE INDISPONIBLE", result); return result; }
  if (!sha256Matches(kPayload, progress)) {
    sceIoRemove(kPayload);
    result = downloadPayload(progress);
    if (result < 0) { setError(error, errorSize, "TELECHARGEMENT ECHEC", result); return result; }
    if (!sha256Matches(kPayload, progress)) {
      sceIoRemove(kPayload);
      setError(error, errorSize, "INTEGRITE NON VALIDE");
      return -1;
    }
  }
  result = extractPayload(progress);
  if (result < 0) setError(error, errorSize, "DEPLOIEMENT ECHEC", result);
  return result;
}
