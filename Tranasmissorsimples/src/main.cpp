#include <WiFi.h>
#include <esp_now.h>
#include <DHT.h>
#include <Preferences.h>
#include <NimBLEDevice.h>

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

void carregarConfig() {
  prefs.begin("vinhaTX", true);

  hasConfig    = prefs.getBool("hasCfg", false);
  masterMacStr = prefs.getString("masterMac", "");
  sensorName   = prefs.getString("nome", "");
  latitudeCfg  = prefs.getFloat("lat", 0.0);
  longitudeCfg = prefs.getFloat("lon", 0.0);

  prefs.end();

  if (hasConfig && parseMacString(masterMacStr, receptorMAC)) {
    Serial.println("Config encontrada:");
    Serial.println("Master MAC: " + masterMacStr);
    Serial.println("Nome: " + sensorName);
    Serial.print("Lat: "); Serial.println(latitudeCfg);
    Serial.print("Lon: "); Serial.println(longitudeCfg);
  } else {
    Serial.println("Nenhuma configuração existente.");
    hasConfig = false;
  }
}

void salvarConfig() {
  prefs.begin("vinhaTX", false);

  prefs.putBool("hasCfg", true);
  prefs.putString("masterMac", masterMacStr);
  prefs.putString("nome",      sensorName);
  prefs.putFloat("lat",        latitudeCfg);
  prefs.putFloat("lon",        longitudeCfg);

  prefs.end();

  Serial.println("Config SALVA com sucesso!");
}

//////////////////// CALLBACK BLE ////////////////////////////
class ConfigCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    String s(value.c_str());

    Serial.print("[BLE] Recebido: ");
    Serial.println(s);

    String macStr, nome;
    float lat, lon;

    if (!parseConfigString(s, macStr, nome, lat, lon)) {
      Serial.println("[ERRO] Formato inválido. Esperado: MAC;NOME;LAT;LON");
      return;
    }

    uint8_t tempMac[6];
    if (!parseMacString(macStr, tempMac)) {
      Serial.println("[ERRO] MAC inválido!");
      return;
    }

    Serial.println("[BLE] Dados válidos recebidos:");
    Serial.println("MAC: " + macStr);
    Serial.println("Nome: " + nome);
    Serial.print("Lat: "); Serial.println(lat);
    Serial.print("Lon: "); Serial.println(lon);

    masterMacStr = macStr;
    sensorName   = nome;
    latitudeCfg  = lat;
    longitudeCfg = lon;
    memcpy(receptorMAC, tempMac, 6);

    salvarConfig();
    novaConfigRecebida = true;
  }
};

//////////////////// BLE CONFIG MODE //////////////////////////
void iniciarBLEConfig() {
  Serial.println("Iniciando BLE (modo CONFIG)...");

  NimBLEDevice::init("TX-VINHA");

  NimBLEServer *server = NimBLEDevice::createServer();
  NimBLEService *service = server->createService(TX_SERVICE_UUID);

  configCharacteristic = service->createCharacteristic(
    TX_CHAR_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
  );

  configCharacteristic->setCallbacks(new ConfigCallback());
  service->start();

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

  Serial.println("\n========== TRANSMISSOR ==========");

  dht.begin();
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

    delay(50);
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

  delay(2000);
}

