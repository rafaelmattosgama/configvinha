#pragma once

#include <Arduino.h>

struct DeviceConfig {
  bool hasConfig = false;
  String masterMacStr;
  String sensorName;
  float latitude = 0.0f;
  float longitude = 0.0f;
  uint8_t masterMac[6] = {0};
};

bool parseMacString(const String &str, uint8_t mac[6]);
bool parseConfigPayload(const String &payload, DeviceConfig &cfg, String &error);

void loadConfig(DeviceConfig &cfg);
void saveConfig(const DeviceConfig &cfg);

String getChipId();

