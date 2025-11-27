#pragma once

#include <functional>

#include <NimBLEDevice.h>

#include "config_storage.h"

class BleConfigServer : public NimBLECharacteristicCallbacks {
 public:
  void begin(DeviceConfig *cfg, std::function<void()> onSaved);
  void stop();
  bool isActive() const { return active_; }

 protected:
  void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo& connInfo) override;

 private:
  DeviceConfig *cfg_ = nullptr;
  std::function<void()> onSaved_;
  bool active_ = false;
};

