#include <NimBLEDevice.h>
#include "esp_mac.h"  // necessário para esp_read_mac e ESP_MAC_WIFI_SOFTAP

#define MASTER_SERVICE_UUID "0000bbbb-0000-1000-8000-00805f9b34fb"
#define MASTER_CHAR_UUID    "0000bb01-0000-1000-8000-00805f9b34fb"

NimBLECharacteristic *macCharacteristic;

String getMacAddress() {
  uint8_t mac[6];
  char macStr[18];

  // Lê o MAC usado pelo WiFi AP (sem ativar o WiFi)
  esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);

  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  return String(macStr);
}

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println("=== MASTER BLE CONFIGURADOR (NimBLE + MAC via EFUSE) ===");

  String mac = getMacAddress();
  Serial.println("MAC do Master: " + mac);

  NimBLEDevice::init("MASTER-VINHA");

  NimBLEServer *server = NimBLEDevice::createServer();
  NimBLEService *service = server->createService(MASTER_SERVICE_UUID);

  macCharacteristic = service->createCharacteristic(
    MASTER_CHAR_UUID,
    NIMBLE_PROPERTY::READ
  );

  macCharacteristic->setValue(mac.c_str());

  service->start();

  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(MASTER_SERVICE_UUID);

  NimBLEDevice::startAdvertising();
  Serial.println("Advertising iniciado.");
}

void loop() {
  delay(1000);
}

