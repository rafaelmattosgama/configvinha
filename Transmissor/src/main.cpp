#include <Arduino.h>
#include <esp_sleep.h>

#include "ble_config.h"
#include "config_defs.h"
#include "config_storage.h"
#include "espnow_comm.h"
#include "sensors.h"

enum class Mode { CONFIG, RUN };

static BleConfigServer bleServer;
static DeviceConfig deviceCfg;
static bool novaConfig = false;
static unsigned long configStartMillis = 0;
static Mode modoAtual = Mode::CONFIG;

static void startConfigMode() {
  Serial.println("Iniciando modo CONFIG (BLE)...");
  bleServer.begin(&deviceCfg, []() { novaConfig = true; });
  configStartMillis = millis();
  modoAtual = Mode::CONFIG;
}

static bool startRunMode() {
  if (modoAtual == Mode::RUN) return true;
  bleServer.stop();

  if (!deviceCfg.hasConfig) {
    Serial.println("Sem configuracao valida. Voltando para modo CONFIG.");
    return false;
  }

  Serial.print("Master MAC: ");
  Serial.println(deviceCfg.masterMacStr);
  Serial.println("Iniciando ESP-NOW...");

  if (!espnowBegin(deviceCfg.masterMac)) {
    Serial.println("Falha ao iniciar ESP-NOW.");
    return false;
  }

  modoAtual = Mode::RUN;
  return true;
}

static void enterDeepSleep() {
  Serial.printf("Dormindo por %u segundos...\n", SLEEP_SECONDS);
  esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(SLEEP_SECONDS) * 1000000ULL);
  esp_deep_sleep_start();
}

static void sendOnceAndSleep() {
  SensorReadings r = readSensors();

  TelemetryPacket pkt;
  String chipId = getChipId();
  strncpy(pkt.id, chipId.c_str(), sizeof(pkt.id) - 1);
  strncpy(pkt.name, deviceCfg.sensorName.c_str(), sizeof(pkt.name) - 1);
  pkt.latitude = deviceCfg.latitude;
  pkt.longitude = deviceCfg.longitude;
  pkt.temperatura = r.temperatura;
  pkt.umidadeAr = r.umidadeAr;
  pkt.soloADC = r.soloADC;
  pkt.tensaoBat = r.tensaoBat;

  Serial.println("Enviando pacote ESP-NOW:");
  Serial.printf("ID: %s | Nome: %s\n", pkt.id, pkt.name);
  Serial.printf("Lat: %.6f Lon: %.6f\n", pkt.latitude, pkt.longitude);
  Serial.printf("Temp: %.2f Umid: %.2f Solo: %d Bat: %.2f\n",
                pkt.temperatura, pkt.umidadeAr, pkt.soloADC, pkt.tensaoBat);

  sendTelemetry(pkt, deviceCfg.masterMac);
  enterDeepSleep();
}

void setup() {
  Serial.begin(115200);
  delay(200);

  Serial.println("===== TRANSMISSOR =====");

  initSensors();
  loadConfig(deviceCfg);

  if (deviceCfg.hasConfig) {
    Serial.println("Config existente carregada.");
    Serial.print("Master: ");
    Serial.println(deviceCfg.masterMacStr);
  } else {
    Serial.println("Nenhuma config armazenada.");
  }

  startConfigMode();
}

void loop() {
  if (modoAtual == Mode::CONFIG) {
    if (novaConfig) {
      Serial.println("Config BLE recebida.");
      novaConfig = false;
      if (startRunMode()) {
        sendOnceAndSleep();
      } else {
        startConfigMode();
      }
      return;
    }

    if (deviceCfg.hasConfig && (millis() - configStartMillis > CONFIG_WINDOW_MS)) {
      Serial.println("Janela de config encerrada. Indo para RUN.");
      if (startRunMode()) {
        sendOnceAndSleep();
      } else {
        startConfigMode();
      }
      return;
    }

    delay(50);
    return;
  }

  // Se por algum motivo estivermos em RUN sem dormir, envie e durma.
  sendOnceAndSleep();
}

