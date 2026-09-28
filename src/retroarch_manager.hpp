#pragma once
#include <stddef.h>

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
int vrEnsureRetroArchData();
int vrLaunchGame(const char* rom_path, char* error, size_t error_size);
