#include "ble_config.h"
#include "app_state.h"
#include "config.h"
#include <NimBLEDevice.h>

//////////////////// UUIDS (ALINHADOS COM SEU APP) /////////////
#define TX_SERVICE_UUID     "0000aaaa-0000-1000-8000-00805f9b34fb"
#define TX_CHAR_UUID        "0000aa01-0000-1000-8000-00805f9b34fb"

static NimBLECharacteristic *configCharacteristic = nullptr;

static void configurarMTU() {
    NimBLEDevice::setMTU(256); // Define o MTU para suportar pacotes maiores
    Serial.println("[LOG] MTU configurado para 256 bytes.");
}

class ConfigCallback : public NimBLECharacteristicCallbacks {
public:
  void onWrite(NimBLECharacteristic *pCharacteristic) override {
    Serial.println("[LOG] Callback BLE acionado. Verificando pacote recebido...");

    std::string value = pCharacteristic->getValue();
    String s(value.c_str());

    if (s.length() == 0) {
      Serial.println("[ERRO] Pacote recebido esta vazio.");
      return;
    }

    Serial.printf("[LOG] Pacote recebido com %d bytes: %s\n", s.length(), s.c_str());
    Serial.print("[LOG] Pacote (hex): ");
    for (size_t i = 0; i < value.size(); i++) {
      Serial.printf("%02X ", (uint8_t)value[i]);
    }
    Serial.println();

    logPacoteRecebido(s);

    processarPayloadConfig(s);
  }
};

class ServerCallbacks : public NimBLEServerCallbacks {
public:
  void onConnect(NimBLEServer *pServer) override {
    Serial.println("[LOG] Dispositivo conectado ao servidor BLE.");
  }

  void onDisconnect(NimBLEServer *pServer) override {
    Serial.println("[LOG] Dispositivo desconectado do servidor BLE.");
    NimBLEDevice::startAdvertising(); // retoma advertising para novo app
  }
};

class GlobalServerCallbacks : public NimBLEServerCallbacks {
public:
  void onWrite(NimBLECharacteristic *pCharacteristic) override {
    Serial.println("[LOG] Evento de escrita capturado no servidor BLE.");

    std::string value = pCharacteristic->getValue();
    String s(value.c_str());

    if (s.length() == 0) {
      Serial.println("[ERRO] Pacote recebido esta vazio no callback global.");
      return;
    }

    Serial.printf("[LOG] Pacote recebido no callback global com %d bytes: %s\n", s.length(), s.c_str());
  }
};

void startBLEConfigMode() {
  Serial.println("Iniciando BLE (modo CONFIG)...");

  NimBLEDevice::init("TX-VINHA");
  configurarMTU(); // Configura o MTU para pacotes maiores

  NimBLEServer *server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks()); // Logs de conexao BLE
  Serial.println("[BLE] Servidor BLE criado.");

  NimBLEService *service = server->createService(TX_SERVICE_UUID);
  Serial.println("[BLE] Servico BLE criado com UUID: " + String(TX_SERVICE_UUID));

  configCharacteristic = service->createCharacteristic(
    TX_CHAR_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR, // compatibilidade: write com e sem resposta
    128 // aumenta maxLen para caber string grande
  );
  Serial.println("[BLE] Caracteristica BLE criada com UUID: " + String(TX_CHAR_UUID));

  configCharacteristic->setCallbacks(new ConfigCallback());
  server->setCallbacks(new GlobalServerCallbacks());
  Serial.println("[BLE] Callback configurado para a caracteristica.");

  service->start();
  Serial.println("[BLE] Servico BLE iniciado.");

  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(TX_SERVICE_UUID);
  NimBLEDevice::startAdvertising();

  Serial.println("BLE Advertising ativo!");
  Serial.println("SERVICOS BLE ATIVOS:");
}

void stopBLE() {
  Serial.println("Desligando BLE...");
  NimBLEDevice::stopAdvertising();
}

void runBLEConfigLoop() {
  // Fallback: se o callback nao disparar, tenta ler a caracteristica e processar
  static String ultimoPayload = "";
  if (configCharacteristic != nullptr && !novaConfigRecebida) {
    std::string raw = configCharacteristic->getValue();
    if (!raw.empty()) {
      String s(raw.c_str());
      if (s != ultimoPayload) {
        Serial.println("[LOG] Detectado valor na caracteristica (polling). Processando...");
        processarPayloadConfig(s);
        ultimoPayload = s;
      }
    }
  }

  // Log BLE periodico para verificar conexoes
  static unsigned long ultimoLogBLE = 0;
  if (millis() - ultimoLogBLE > 5000) {
    int conexoes = NimBLEDevice::getServer()->getConnectedCount();
    Serial.printf("[LOG] Aguardando config BLE. Conexoes ativas: %d\n", conexoes);
    ultimoLogBLE = millis();
  }
}
