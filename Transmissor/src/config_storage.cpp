#include "config_storage.h"

#include <Preferences.h>

#include "config_defs.h"

static Preferences prefs;

bool parseMacString(const String &str, uint8_t mac[6]) {
  int b[6];
  if (sscanf(str.c_str(), "%x:%x:%x:%x:%x:%x",
             &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) == 6) {
    for (int i = 0; i < 6; i++) mac[i] = static_cast<uint8_t>(b[i]);
    return true;
  }
  return false;
}

static bool parseConfigString(const String &s, String &macStr, String &nome, float &lat, float &lon) {
  int p1 = s.indexOf(';');
  int p2 = s.indexOf(';', p1 + 1);
  int p3 = s.indexOf(';', p2 + 1);

  if (p1 == -1 || p2 == -1 || p3 == -1) return false;

  macStr = s.substring(0, p1);
  nome = s.substring(p1 + 1, p2);
  lat = s.substring(p2 + 1, p3).toFloat();
  lon = s.substring(p3 + 1).toFloat();

  return true;
}

bool parseConfigPayload(const String &payload, DeviceConfig &cfg, String &error) {
  String macStr, nome;
  float lat, lon;

  if (!parseConfigString(payload, macStr, nome, lat, lon)) {
    error = "Formato invalido. Use MAC;NOME;LAT;LON";
    return false;
  }

  uint8_t tempMac[6];
  if (!parseMacString(macStr, tempMac)) {
    error = "MAC invalido";
    return false;
  }

  cfg.masterMacStr = macStr;
  cfg.sensorName = nome;
  cfg.latitude = lat;
  cfg.longitude = lon;
  memcpy(cfg.masterMac, tempMac, sizeof(tempMac));
  cfg.hasConfig = true;
  return true;
}

void loadConfig(DeviceConfig &cfg) {
  prefs.begin(PREF_NAMESPACE, true);

  cfg.hasConfig = prefs.getBool(PREF_KEY_HASCFG, false);
  cfg.masterMacStr = prefs.getString(PREF_KEY_MAC, "");
  cfg.sensorName = prefs.getString(PREF_KEY_NAME, "");
  cfg.latitude = prefs.getFloat(PREF_KEY_LAT, 0.0f);
  cfg.longitude = prefs.getFloat(PREF_KEY_LON, 0.0f);

  prefs.end();

  if (!cfg.hasConfig) return;

  if (!parseMacString(cfg.masterMacStr, cfg.masterMac)) {
    cfg.hasConfig = false;
  }
}

void saveConfig(const DeviceConfig &cfg) {
  prefs.begin(PREF_NAMESPACE, false);

  prefs.putBool(PREF_KEY_HASCFG, cfg.hasConfig);
  prefs.putString(PREF_KEY_MAC, cfg.masterMacStr);
  prefs.putString(PREF_KEY_NAME, cfg.sensorName);
  prefs.putFloat(PREF_KEY_LAT, cfg.latitude);
  prefs.putFloat(PREF_KEY_LON, cfg.longitude);

  prefs.end();
}

String getChipId() {
  uint64_t mac = ESP.getEfuseMac();
  char buf[17];
  snprintf(buf, sizeof(buf), "%012llX", static_cast<unsigned long long>(mac));
  return String(buf);
}

