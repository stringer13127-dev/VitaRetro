#include "retroarch_manager.hpp"
#include <psp2/appmgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <stdio.h>
#include <string.h>

static bool endsWithNoCase(const char* s, const char* suffix) {
  if (!s || !suffix) return false;
  size_t ls = strlen(s), lf = strlen(suffix);
  if (lf > ls) return false;
  const char* a = s + (ls - lf);
  for (size_t i = 0; i < lf; ++i) {
    char x = a[i], y = suffix[i];
    if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
    if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
    if (x != y) return false;
  }
  return true;
}

bool vrFileExists(const char* path) {
  SceIoStat st;
  memset(&st, 0, sizeof(st));
  return path && sceIoGetstat(path, &st) >= 0;
}

VrSystem vrDetectSystemFromPath(const char* path) {
  if (!path) return VrSystem::Unknown;
  if (endsWithNoCase(path, ".sfc") || endsWithNoCase(path, ".smc")) return VrSystem::Snes;
  if (endsWithNoCase(path, ".md") || endsWithNoCase(path, ".gen") || endsWithNoCase(path, ".bin")) return VrSystem::MegaDrive;
  if (endsWithNoCase(path, ".nes")) return VrSystem::Nes;
  if (endsWithNoCase(path, ".gb") || endsWithNoCase(path, ".gbc")) return VrSystem::GameBoy;
  if (endsWithNoCase(path, ".gba")) return VrSystem::GameBoyAdvance;
  if (endsWithNoCase(path, ".cue") || endsWithNoCase(path, ".chd") || endsWithNoCase(path, ".pbp")) return VrSystem::Playstation1;
  return VrSystem::Unknown;
}

VrLaunchPlan vrResolveLaunchPlan(VrSystem system) {
  switch (system) {
    case VrSystem::Snes: return {system, "app0:/retroarch/snes9x2005_plus_libretro.self", "SNES"};
    case VrSystem::MegaDrive: return {system, "app0:/retroarch/genesis_plus_gx_libretro.self", "MEGA DRIVE"};
    case VrSystem::Nes: return {system, "app0:/retroarch/fceumm_libretro.self", "NES"};
    case VrSystem::GameBoy: return {system, "app0:/retroarch/gambatte_libretro.self", "GAME BOY"};
    case VrSystem::GameBoyAdvance: return {system, "app0:/retroarch/gpsp_libretro.self", "GAME BOY ADVANCE"};
    case VrSystem::Playstation1: return {system, "app0:/retroarch/pcsx_rearmed_libretro.self", "PLAYSTATION"};
    default: return {VrSystem::Unknown, nullptr, "INCONNU"};
  }
}

bool vrRetroArchPayloadPresent() {
  const char* required[] = {
    "app0:/retroarch/snes9x2005_plus_libretro.self",
    "app0:/retroarch/genesis_plus_gx_libretro.self",
    "app0:/retroarch/fceumm_libretro.self",
    "app0:/retroarch/gambatte_libretro.self",
    "app0:/retroarch/gpsp_libretro.self",
    "app0:/retroarch/pcsx_rearmed_libretro.self"
  };
  for (unsigned i=0;i<sizeof(required)/sizeof(required[0]);++i) {
    if (!vrFileExists(required[i])) return false;
  }
  return true;
}

static int makeDirIfNeeded(const char* path) {
  int r = sceIoMkdir(path, 0777);
  if (r >= 0) return 0;
  SceIoStat st;
  memset(&st, 0, sizeof(st));
  return sceIoGetstat(path, &st) >= 0 ? 0 : r;
}

static int copyFile(const char* from, const char* to) {
  // Preserve files from an existing RetroArch installation, including settings.
  if (vrFileExists(to)) return 0;
  char temporary[512];
  int length = snprintf(temporary, sizeof(temporary), "%s.vr-tmp", to);
  if (length < 0 || length >= (int)sizeof(temporary)) return -1;
  SceUID input = sceIoOpen(from, SCE_O_RDONLY, 0);
  if (input < 0) return input;
  SceUID output = sceIoOpen(temporary, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
  if (output < 0) { sceIoClose(input); return output; }
  char buffer[16384];
  int result = 0, count;
  while ((count = sceIoRead(input, buffer, sizeof(buffer))) > 0) {
    int written = 0;
    while (written < count) {
      int n = sceIoWrite(output, buffer + written, count - written);
      if (n <= 0) { result = n < 0 ? n : -1; break; }
      written += n;
    }
    if (result < 0) break;
  }
  if (count < 0 && result == 0) result = count;
  sceIoClose(output);
  sceIoClose(input);
  if (result == 0) result = sceIoRename(temporary, to);
  if (result < 0) sceIoRemove(temporary);
  return result;
}

static int copyTree(const char* from, const char* to) {
  int result = makeDirIfNeeded(to);
  if (result < 0) return result;
  SceUID dir = sceIoDopen(from);
  if (dir < 0) return dir;
  SceIoDirent entry;
  memset(&entry, 0, sizeof(entry));
  int n = 0;
  while ((n = sceIoDread(dir, &entry)) > 0) {
    if (strcmp(entry.d_name, ".") == 0 || strcmp(entry.d_name, "..") == 0) continue;
    char source[512], target[512];
    int sl = snprintf(source, sizeof(source), "%s/%s", from, entry.d_name);
    int tl = snprintf(target, sizeof(target), "%s/%s", to, entry.d_name);
    if (sl < 0 || sl >= (int)sizeof(source) || tl < 0 || tl >= (int)sizeof(target)) {
      result = -1; break;
    }
    SceUID child = sceIoDopen(source);
    if (child >= 0) {
      sceIoDclose(child);
      result = copyTree(source, target);
    } else {
      result = copyFile(source, target);
    }
    if (result < 0) break;
    memset(&entry, 0, sizeof(entry));
  }
  if (n < 0 && result == 0) result = n;
  sceIoDclose(dir);
  return result;
}

int vrEnsureRetroArchData() {
  if (makeDirIfNeeded("ux0:/data/VitaRetro") < 0 ||
      makeDirIfNeeded("ux0:/data/VitaRetro/roms") < 0) return -1;
  const char* marker = "ux0:/data/retroarch/.vitaretro_1_22_2";
  if (vrFileExists(marker)) return 0;
  int result = copyTree("app0:/retroarch-data", "ux0:/data/retroarch");
  if (result < 0) return result;
  SceUID done = sceIoOpen(marker, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
  if (done < 0) return done;
  sceIoClose(done);
  return 0;
}

int vrLaunchGame(const char* rom_path, char* error, size_t error_size) {
  if (error && error_size) error[0] = 0;
  if (!rom_path || !vrFileExists(rom_path)) {
    if (error && error_size) snprintf(error, error_size, "ROM INTROUVABLE");
    return -1;
  }

  VrSystem system = vrDetectSystemFromPath(rom_path);
  VrLaunchPlan plan = vrResolveLaunchPlan(system);
  if (!plan.core_self) {
    if (error && error_size) snprintf(error, error_size, "FORMAT NON PRIS EN CHARGE");
    return -2;
  }
  if (!vrFileExists(plan.core_self)) {
    if (error && error_size) snprintf(error, error_size, "CORE RETROARCH ABSENT");
    return -3;
  }
  if (vrEnsureRetroArchData() < 0) {
    if (error && error_size) snprintf(error, error_size, "DONNEES RETROARCH INDISPONIBLES");
    return -4;
  }

  // RetroArch Vita cores are statically-linked SELF executables. They can be
  // launched from the same installed application (app0:) with the content path
  // supplied as argv. This keeps the normal user flow inside VitaRetro.
  char* argv[3];
  argv[0] = const_cast<char*>(plan.core_self);
  argv[1] = const_cast<char*>(rom_path);
  argv[2] = nullptr;
  int r = sceAppMgrLoadExec(plan.core_self, argv, nullptr);
  if (r < 0 && error && error_size) snprintf(error, error_size, "ECHEC LANCEMENT %08X", (unsigned)r);
  return r;
}
