#include "ble_config.h"

#include "config_defs.h"
#include "config_storage.h"

void BleConfigServer::begin(DeviceConfig *cfg, std::function<void()> onSaved) {
  cfg_ = cfg;
  onSaved_ = onSaved;

  NimBLEDevice::init(BLE_DEVICE_NAME);

  NimBLEServer *server = NimBLEDevice::createServer();
  NimBLEService *service = server->createService(TX_SERVICE_UUID);

  NimBLECharacteristic *configChar = service->createCharacteristic(
      TX_CHAR_UUID,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);

  configChar->setCallbacks(this);
  service->start();

  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(TX_SERVICE_UUID);
  NimBLEDevice::startAdvertising();

  active_ = true;
  Serial.println("BLE configuracao ativo.");
}

void BleConfigServer::stop() {
  if (!active_) return;
  NimBLEDevice::stopAdvertising();
  active_ = false;
}

void BleConfigServer::onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo& connInfo) {
  std::string value = pCharacteristic->getValue();
  String payload(value.c_str());

  Serial.print("[BLE] Recebido: ");
  Serial.println(payload);

  DeviceConfig temp = *cfg_;
  String error;
  if (!parseConfigPayload(payload, temp, error)) {
    Serial.print("[BLE] Erro: ");
    Serial.println(error);
    return;
  }

  *cfg_ = temp;
  saveConfig(*cfg_);

  if (onSaved_) onSaved_();
}

