#pragma once

#include <Arduino.h>

struct TelemetryPacket {
  char id[18] = {0};     // chip ID (12 hex) + null
  char name[32] = {0};   // nome configurado
  float latitude = 0.0f;
  float longitude = 0.0f;
  float temperatura = 0.0f;
  float umidadeAr = 0.0f;
  int32_t soloADC = 0;
  float tensaoBat = 0.0f;
};

bool espnowBegin(const uint8_t peerMac[6]);
bool sendTelemetry(const TelemetryPacket &pkt, const uint8_t peerMac[6]);

