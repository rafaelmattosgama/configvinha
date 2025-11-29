#include <WiFi.h>
#include <esp_now.h>
#include <DHT.h>
#include <Preferences.h>
#include <NimBLEDevice.h>
#include <nvs_flash.h>
#include <EEPROM.h>

//////////////////////////// PINOS ////////////////////////////
#define DHTPIN 22
#define DHTTYPE DHT11
#define SOIL_PIN 32
#define BAT_PIN 34

DHT dht(DHTPIN, DHTTYPE);

//////////////////// STRUCT ESP-NOW ///////////////////////////
typedef struct {
  float temperatura;
  float umidadeAr;
  int   soloADC;
  float tensaoBat;
} DadosEnvio;

DadosEnvio pacote;

//////////////////// FLASH (NVS) //////////////////////////////
Preferences prefs;

String masterMacStr = "";
String sensorName   = "";
float latitudeCfg   = 0;
float longitudeCfg  = 0;
bool hasConfig      = false;

uint8_t receptorMAC[6] = {0};

//////////////////// ESTADOS /////////////////////////////////
enum Modo { MODO_CONFIG, MODO_RUN };
Modo modoAtual = MODO_CONFIG;

unsigned long configStartMillis = 0;
const unsigned long CONFIG_WINDOW_MS = 30000;  // 30s para reconfig

bool novaConfigRecebida = false;

//////////////////// UUIDS (ALINHADOS COM SEU APP) /////////////
#define TX_SERVICE_UUID     "0000aaaa-0000-1000-8000-00805f9b34fb"
#define TX_CHAR_UUID        "0000aa01-0000-1000-8000-00805f9b34fb"

NimBLECharacteristic *configCharacteristic;

//////////////////// FUNÇÕES AUXILIARES ///////////////////////
bool parseMacString(const String &str, uint8_t mac[6]) {
  int b[6];
  if (sscanf(str.c_str(), "%x:%x:%x:%x:%x:%x",
             &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) == 6) {
    for (int i = 0; i < 6; i++) mac[i] = (uint8_t)b[i];
    return true;
  }
  return false;
}

bool parseConfigString(const String &s, String &macStr, String &nome, float &lat, float &lon) {
  int p1 = s.indexOf(';');
  int p2 = s.indexOf(';', p1 + 1);
  int p3 = s.indexOf(';', p2 + 1);

  if (p1 == -1 || p2 == -1 || p3 == -1) return false;

  macStr = s.substring(0, p1);
  nome   = s.substring(p1 + 1, p2);
  lat    = s.substring(p2 + 1, p3).toFloat();
  lon    = s.substring(p3 + 1).toFloat();

  return true;
}

#define EEPROM_SIZE 64  // Tamanho da EEPROM

void salvarMACNaEEPROM(const uint8_t mac[6]) {
    for (int i = 0; i < 6; i++) {
        EEPROM.write(i, mac[i]);
    }
    EEPROM.commit();
    Serial.println("MAC salvo na EEPROM.");
}

bool carregarMACDaEEPROM(uint8_t mac[6]) {
    for (int i = 0; i < 6; i++) {
        mac[i] = EEPROM.read(i);
    }

    // Verificar se o MAC é válido (não vazio)
    for (int i = 0; i < 6; i++) {
        if (mac[i] != 0xFF) {
            return true;
        }
    }
    return false;
}

void carregarConfig() {
  Serial.println("Carregando configuração...");

  if (carregarMACDaEEPROM(receptorMAC)) {
      hasConfig = true;
      Serial.println("Config encontrada na EEPROM:");
      Serial.print("Master MAC: ");
      for (int i = 0; i < 6; i++) {
          Serial.printf("%02X:", receptorMAC[i]);
      }
      Serial.println();
  } else {
      Serial.println("Nenhuma configuração existente na EEPROM.");
      hasConfig = false;
  }
}

void salvarConfig() {
  Serial.println("Salvando configuração...");

  if (masterMacStr.length() > 0 && parseMacString(masterMacStr, receptorMAC)) {
          salvarMACNaEEPROM(receptorMAC);
          hasConfig = true;
          Serial.println("Config SALVA com sucesso na EEPROM!");
    } else {
        Serial.println("[ERRO] Falha ao salvar configuração. MAC inválido.");
    }
}

//////////////////// LOGS DETALHADOS //////////////////////////
void logEstadoMemoria() {
    Serial.println("[LOG] Verificando estado da memória EEPROM...");
    uint8_t mac[6];
    if (carregarMACDaEEPROM(mac)) {
        Serial.print("[LOG] MAC encontrado na EEPROM: ");
        for (int i = 0; i < 6; i++) {
            Serial.printf("%02X:", mac[i]);
        }
        Serial.println();
    } else {
        Serial.println("[LOG] Nenhum MAC encontrado na EEPROM.");
    }
}

void logPacoteRecebido(const String &s) {
    Serial.println("[LOG] Pacote recebido via BLE:");
    Serial.println(s);
}

void logConfigSalva() {
    Serial.println("[LOG] Configuração salva na EEPROM com sucesso.");
}

//////////////////// LOGS DE CONEXÃO BLE //////////////////////////
void logBLEConexao() {
    Serial.println("[LOG] Verificando conexão BLE...");
    if (NimBLEDevice::getServer()->getConnectedCount() > 0) {
        Serial.println("[LOG] Dispositivo conectado via BLE.");
    } else {
        Serial.println("[LOG] Nenhum dispositivo conectado via BLE.");
    }
}

//////////////////// CONFIGURAR MTU //////////////////////////
void configurarMTU() {
    NimBLEDevice::setMTU(256); // Define o MTU para suportar pacotes maiores
    Serial.println("[LOG] MTU configurado para 256 bytes.");
}

//////////////////// CALLBACK BLE ////////////////////////////
class ConfigCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic) {
    Serial.println("[LOG] Callback BLE acionado. Verificando pacote recebido...");

    std::string value = pCharacteristic->getValue();
    String s(value.c_str());

    if (s.length() == 0) {
      Serial.println("[ERRO] Pacote recebido está vazio.");
      return;
    }

    Serial.printf("[LOG] Pacote recebido com %d bytes: %s\n", s.length(), s.c_str());

    logPacoteRecebido(s);

    String macStr, nome;
    float lat = 0, lon = 0;

    bool formatoCompleto = parseConfigString(s, macStr, nome, lat, lon);

    uint8_t tempMac[6];
    if (!formatoCompleto) {
      // Fallback: aceita payload apenas com o MAC para contornar clientes com MTU baixo
      // (ex.: write without response no Web Bluetooth limita o tamanho em ~20 bytes).
      int sep = s.indexOf(';');
      macStr = (sep == -1) ? s : s.substring(0, sep);
      macStr.trim();
      if (!parseMacString(macStr, tempMac)) {
        Serial.println("[ERRO] Formato invalido. Esperado: MAC;NOME;LAT;LON ou apenas MAC.");
        return;
      }
      Serial.println("[BLE] Payload curto recebido; usando apenas o MAC.");
      nome = "";
      lat = 0;
      lon = 0;
    } else {
      if (!parseMacString(macStr, tempMac)) {
        Serial.println("[ERRO] MAC invalido!");
        return;
      }

      Serial.println("[BLE] Dados validos recebidos:");
      Serial.println("MAC: " + macStr);
      Serial.println("Nome: " + nome);
      Serial.print("Lat: "); Serial.println(lat);
      Serial.print("Lon: "); Serial.println(lon);
    }

    masterMacStr = macStr;
    sensorName   = nome;
    latitudeCfg  = lat;
    longitudeCfg = lon;
    memcpy(receptorMAC, tempMac, 6);
    salvarConfig();
    logConfigSalva();
    novaConfigRecebida = true;
  }
};

//////////////////// CALLBACK DE CONEXÃO BLE //////////////////////////
class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *pServer) {
    Serial.println("[LOG] Dispositivo conectado ao servidor BLE.");
  }

  void onDisconnect(NimBLEServer *pServer) {
    Serial.println("[LOG] Dispositivo desconectado do servidor BLE.");
  }
};

//////////////////// CALLBACK GLOBAL BLE //////////////////////////
class GlobalServerCallbacks : public NimBLEServerCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic) {
    Serial.println("[LOG] Evento de escrita capturado no servidor BLE.");

    std::string value = pCharacteristic->getValue();
    String s(value.c_str());

    if (s.length() == 0) {
      Serial.println("[ERRO] Pacote recebido está vazio no callback global.");
      return;
    }

    Serial.printf("[LOG] Pacote recebido no callback global com %d bytes: %s\n", s.length(), s.c_str());
  }
};

//////////////////// BLE CONFIG MODE //////////////////////////
void iniciarBLEConfig() {
  Serial.println("Iniciando BLE (modo CONFIG)...");

  NimBLEDevice::init("TX-VINHA");
  configurarMTU(); // Configura o MTU para pacotes maiores

  NimBLEServer *server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks()); // Logs de conexao BLE
  Serial.println("[BLE] Servidor BLE criado.");

  NimBLEService *service = server->createService(TX_SERVICE_UUID);
  Serial.println("[BLE] Serviço BLE criado com UUID: " + String(TX_SERVICE_UUID));

  configCharacteristic = service->createCharacteristic(
    TX_CHAR_UUID,
    NIMBLE_PROPERTY::WRITE // usar somente write com resposta para evitar truncar payloads maiores
  );
  Serial.println("[BLE] Característica BLE criada com UUID: " + String(TX_CHAR_UUID));

  configCharacteristic->setCallbacks(new ConfigCallback());
  Serial.println("[BLE] Callback configurado para a característica.");

  service->start();
  Serial.println("[BLE] Serviço BLE iniciado.");

  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(TX_SERVICE_UUID);
  NimBLEDevice::startAdvertising();

  Serial.println("BLE Advertising ativo!");
  Serial.println("SERVIÇOS BLE ATIVOS:");
}

void pararBLEConfig() {
  Serial.println("Desligando BLE...");
  NimBLEDevice::stopAdvertising();
}

//////////////////// ESP-NOW /////////////////////////////////
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Enviado para: ");
  for (int i = 0; i < 6; i++) Serial.printf("%02X:", mac_addr[i]);

  Serial.print("  Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FALHOU");
}

bool iniciarEspNow() {
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

//////////////////// SETUP /////////////////////////////////
void setup() {
    Serial.begin(115200);
    delay(500);

    // Inicializar a EEPROM
    if (!EEPROM.begin(EEPROM_SIZE)) {
        Serial.println("[ERRO] Falha ao inicializar EEPROM.");
        while (true);
    }

    Serial.println("\n========== TRANSMISSOR ==========");

    dht.begin();
    logEstadoMemoria();
    carregarConfig();

    iniciarBLEConfig();
    configStartMillis = millis();
}

//////////////////// LOOP /////////////////////////////////
void loop() {
  // ---------------- MODO CONFIG -----------------
  if (modoAtual == MODO_CONFIG) {

    if (novaConfigRecebida) {
      Serial.println("Config BLE recebida! Entrando em modo RUN...");
      pararBLEConfig();
      iniciarEspNow();
      modoAtual = MODO_RUN;
      return;
    }

    if (hasConfig && (millis() - configStartMillis > CONFIG_WINDOW_MS)) {
      Serial.println("Janela de config encerrada. Indo para RUN.");
      pararBLEConfig();
      iniciarEspNow();
      modoAtual = MODO_RUN;
      return;
    }

    // Removido o delay e os logs repetitivos
    return;
  }

  // ---------------- MODO RUN -----------------
  float temperatura = dht.readTemperature();
  float umidadeAr   = dht.readHumidity();
  int soloADC       = analogRead(SOIL_PIN);

  int batRaw = analogRead(BAT_PIN);
  float batVoltage = batRaw * (4.2 / 4095.0);

  pacote.temperatura = temperatura;
  pacote.umidadeAr   = umidadeAr;
  pacote.soloADC     = soloADC;
  pacote.tensaoBat   = batVoltage;

  esp_now_send(receptorMAC, (uint8_t *)&pacote, sizeof(pacote));

  Serial.println("---------------");
  Serial.printf("Temp: %.2f\n", temperatura);
  Serial.printf("Umidade: %.2f\n", umidadeAr);
  Serial.printf("Solo ADC: %d\n", soloADC);
  Serial.printf("Bateria: %.2f V\n", batVoltage);

  // Removido o log repetitivo do loop RUN
}


