#pragma once
#include <stddef.h>
#include <stdint.h>

enum class VrSystem {
  Unknown,
  Snes,
  MegaDrive,
  Nes,
  GameBoy,
  GameBoyAdvance,
  Playstation1
};

struct VrLaunchPlan {
  VrSystem system;
  const char* core_self;
  const char* system_name;
};

VrSystem vrDetectSystemFromPath(const char* path);
VrLaunchPlan vrResolveLaunchPlan(VrSystem system);
bool vrFileExists(const char* path);
bool vrRetroArchPayloadPresent();
bool vrExternalRetroArchPresent();
typedef void (*VrDeployProgress)(const char* stage, uint64_t completed, uint64_t total);
int vrDeployRetroArch(VrDeployProgress progress, char* error, size_t error_size);
int vrLaunchGame(const char* rom_path, char* error, size_t error_size);
