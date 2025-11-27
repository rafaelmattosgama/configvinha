#include "espnow_comm.h"

#include <WiFi.h>
#include <esp_now.h>

static bool peerAdded = false;

static void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Enviado para: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", mac_addr[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.print(" Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FALHOU");
}

bool espnowBegin(const uint8_t peerMac[6]) {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ao iniciar ESP-NOW.");
    return false;
  }

  esp_now_register_send_cb(onDataSent);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, peerMac, 6);
  peer.channel = 0;
  peer.encrypt = false;

  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("Erro ao adicionar peer ESP-NOW.");
    return false;
  }

  peerAdded = true;
  Serial.println("ESP-NOW pronto.");
  return true;
}

bool sendTelemetry(const TelemetryPacket &pkt, const uint8_t peerMac[6]) {
  if (!peerAdded) {
    if (!espnowBegin(peerMac)) return false;
  }

  esp_err_t res = esp_now_send(peerMac, reinterpret_cast<const uint8_t *>(&pkt), sizeof(pkt));
  if (res != ESP_OK) {
    Serial.printf("Erro ao enviar ESP-NOW: %d\n", res);
    return false;
  }
  return true;
}

