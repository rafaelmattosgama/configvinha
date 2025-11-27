#pragma once

#include <Arduino.h>

// Pinos de sensores
#define DHTPIN 22
#define DHTTYPE DHT11
#define SOIL_PIN 32
#define BAT_PIN 34

// BLE (config)
#define TX_SERVICE_UUID "0000aaaa-0000-1000-8000-00805f9b34fb"
#define TX_CHAR_UUID "0000aa01-0000-1000-8000-00805f9b34fb"
#define BLE_DEVICE_NAME "TX-VINHA"

// Janela e sono
constexpr uint32_t CONFIG_WINDOW_MS = 30000;  // 30s para reconfigurar quando ja existe config
constexpr uint32_t SLEEP_SECONDS = 300;       // tempo de deep sleep apos envio (5 min)

// NVS
constexpr const char *PREF_NAMESPACE = "vinhaTX";
constexpr const char *PREF_KEY_HASCFG = "hasCfg";
constexpr const char *PREF_KEY_MAC = "masterMac";
constexpr const char *PREF_KEY_NAME = "nome";
constexpr const char *PREF_KEY_LAT = "lat";
constexpr const char *PREF_KEY_LON = "lon";
