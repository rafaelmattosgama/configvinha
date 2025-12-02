#include <Arduino.h>
#include "app_state.h"
#include "config.h"
#include "ble_config.h"
#include "espnow.h"
#include "sensors.h"

unsigned long configStartMillis = 0;
const unsigned long CONFIG_WINDOW_MS = 30000;  // 30s para reconfig

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n========== TRANSMISSOR ==========");

    if (!initConfigStorage()) {
        while (true) {}
    }

    initSensors();
    logEstadoMemoria();
    carregarConfig();

    startBLEConfigMode();
    configStartMillis = millis();
}

void loop() {
  // ---------------- MODO CONFIG -----------------
  if (modoAtual == MODO_CONFIG) {

    runBLEConfigLoop();

    if (novaConfigRecebida) {
      Serial.println("Config BLE recebida! Entrando em modo RUN...");
      stopBLE();
      startEspNow();
      modoAtual = MODO_RUN;
      return;
    }

    if (hasConfig && (millis() - configStartMillis > CONFIG_WINDOW_MS)) {
      Serial.println("Janela de config encerrada. Indo para RUN.");
      stopBLE();
      startEspNow();
      modoAtual = MODO_RUN;
      return;
    }

    return;
  }

  // ---------------- MODO RUN -----------------
  DadosEnvio pacote;
  readAllSensors(pacote);

  sendEspNow(pacote);

  Serial.printf("Temp: %.2f, Umidade: %.2f, Solo: %d, Bateria: %.2fV, Nome: %s, Lat: %.6f, Lon: %.6f\n",
                pacote.temperatura, pacote.umidadeAr, pacote.soloADC, pacote.tensaoBat,
                pacote.nome, pacote.latitude, pacote.longitude);
}
