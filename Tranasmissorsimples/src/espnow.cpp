#include "espnow.h"
#include "app_state.h"
#include <WiFi.h>
#include <esp_now.h>

static void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Enviado para: ");
  for (int i = 0; i < 6; i++) Serial.printf("%02X:", mac_addr[i]);

  Serial.print("  Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FALHOU");
}

bool startEspNow() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ao iniciar ESP-NOW!");
    return false;
  }

  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, receptorMAC, 6);
  peer.channel = 0;
  peer.encrypt = false;

  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("Erro ao adicionar peer.");
    return false;
  }

  Serial.println("ESP-NOW pronto!");
  return true;
}

void sendEspNow(const DadosEnvio &pacote) {
  esp_now_send(receptorMAC, (const uint8_t *)&pacote, sizeof(pacote));
}
