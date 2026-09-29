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
  if (!vrFileExists("ux0:/data/VitaRetro/.retroarch_deployed_1_22_2")) {
    if (error && error_size) snprintf(error, error_size, "DEPLOIE RETROARCH D ABORD");
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
