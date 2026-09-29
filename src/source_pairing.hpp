#pragma once

#include <stdint.h>

struct VrSource {
  char url[512];
  char name[32];
};

void vrSourcesLoad();
const VrSource& vrSource(int slot);
bool vrSourceSet(int slot, const char* url);

// A temporary HTTP form served directly by the Vita on the local Wi-Fi.
// The QR contains a random, session-scoped path; no external account is needed.
bool vrPairStart(int slot);
void vrPairPump();
void vrPairStop();
const char* vrPairUrl();
const char* vrPairStatus();
const uint8_t* vrPairQr();
bool vrPairReceived();
