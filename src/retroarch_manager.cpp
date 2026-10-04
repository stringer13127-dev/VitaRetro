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

static const char* externalCorePath(VrSystem system) {
  switch (system) {
    case VrSystem::Snes: return "snes9x2005_plus_libretro.self";
    case VrSystem::MegaDrive: return "genesis_plus_gx_libretro.self";
    case VrSystem::Nes: return "fceumm_libretro.self";
    case VrSystem::GameBoy: return "gambatte_libretro.self";
    case VrSystem::GameBoyAdvance: return "gpsp_libretro.self";
    case VrSystem::Playstation1: return "pcsx_rearmed_libretro.self";
    default: return nullptr;
  }
}

VrLaunchPlan vrResolveLaunchPlan(VrSystem system) {
  const char* name = externalCorePath(system);
  if (!name) return {VrSystem::Unknown, nullptr, "INCONNU"};

  static char externalPath[160];
  const char* roots[] = {"ux0:/app/RETROVITA/", "ux0:/app/RETROARCH/"};
  for (unsigned i = 0; i < 2; ++i) {
    snprintf(externalPath, sizeof(externalPath), "%s%s", roots[i], name);
    if (vrFileExists(externalPath))
      return {system, externalPath, system == VrSystem::Snes ? "SNES" :
        system == VrSystem::MegaDrive ? "MEGA DRIVE" :
        system == VrSystem::Nes ? "NES" :
        system == VrSystem::GameBoy ? "GAME BOY" :
        system == VrSystem::GameBoyAdvance ? "GAME BOY ADVANCE" : "PLAYSTATION"};
  }

  static char bundledPath[160];
  snprintf(bundledPath, sizeof(bundledPath), "app0:/retroarch/%s", name);
  return {system, bundledPath, system == VrSystem::Snes ? "SNES" :
    system == VrSystem::MegaDrive ? "MEGA DRIVE" :
    system == VrSystem::Nes ? "NES" :
    system == VrSystem::GameBoy ? "GAME BOY" :
    system == VrSystem::GameBoyAdvance ? "GAME BOY ADVANCE" : "PLAYSTATION"};
}

static bool coresPresentAt(const char* root) {
  const char* names[] = {
    "snes9x2005_plus_libretro.self",
    "genesis_plus_gx_libretro.self",
    "fceumm_libretro.self",
    "gambatte_libretro.self",
    "gpsp_libretro.self",
    "pcsx_rearmed_libretro.self"
  };
  for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
    char path[160];
    snprintf(path, sizeof(path), "%s%s", root, names[i]);
    if (!vrFileExists(path)) return false;
  }
  return true;
}

bool vrExternalRetroArchPresent() {
  return coresPresentAt("ux0:/app/RETROVITA/") || coresPresentAt("ux0:/app/RETROARCH/");
}

bool vrRetroArchPayloadPresent() {
  return coresPresentAt("app0:/retroarch/");
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
  // An existing official RetroArch Vita installation is valid; VitaRetro
  // must not force a second download/deployment just to launch a core.
  if (!vrExternalRetroArchPresent() &&
      !vrFileExists("ux0:/data/VitaRetro/.retroarch_deployed_1_22_2")) {
    if (error && error_size) snprintf(error, error_size, "RETROARCH NON CONFIGURE");
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
