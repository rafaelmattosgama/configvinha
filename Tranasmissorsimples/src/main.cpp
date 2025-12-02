// Removed unused includes and cleaned up the code.
#include <WiFi.h>
#include <esp_now.h>
#include <DHT.h>
#include <Preferences.h>
#include <NimBLEDevice.h>
#include <EEPROM.h>

//////////////////////////// PINOS ////////////////////////////
// Define os pinos para o sensor DHT, sensor de solo e bateria.
#define DHTPIN 22
#define DHTTYPE DHT11
#define SOIL_PIN 32
#define BAT_PIN 34

// Inicializa o sensor DHT com o pino e tipo especificados.
DHT dht(DHTPIN, DHTTYPE);

//////////////////// STRUCT ESP-NOW ///////////////////////////
// Define uma estrutura para armazenar os dados que serão enviados via ESP-NOW.
typedef struct {
  float temperatura; // Temperatura medida pelo sensor DHT.
  float umidadeAr;   // Umidade do ar medida pelo sensor DHT.
  int   soloADC;     // Leitura do sensor de solo (valor ADC).
  float tensaoBat;   // Tensão da bateria calculada.
  char  nome[32];    // Nome do sensor.
  float latitude;     // Latitude configurada.
  float longitude;    // Longitude configurada.
} DadosEnvio;

// Cria uma instância da estrutura para armazenar os dados a serem enviados.
DadosEnvio pacote;

//////////////////// FLASH (NVS) //////////////////////////////
// Inicializa a classe Preferences para armazenamento em flash.
Preferences prefs;

// Variáveis globais para armazenar configurações e estado do dispositivo.
String masterMacStr = ""; // Endereço MAC do receptor em formato string.
String sensorName   = ""; // Nome do sensor configurado.
float latitudeCfg   = 0;  // Latitude configurada.
float longitudeCfg  = 0;  // Longitude configurada.
bool hasConfig      = false; // Indica se há configuração salva.

// Array para armazenar o endereço MAC do receptor em formato binário.
uint8_t receptorMAC[6] = {0};

//////////////////// ESTADOS /////////////////////////////////
// Define os modos de operação do dispositivo.
enum Modo { MODO_CONFIG, MODO_RUN };
Modo modoAtual = MODO_CONFIG; // Define o modo inicial como configuração.

// Variáveis para controlar o tempo de configuração.
unsigned long configStartMillis = 0;
const unsigned long CONFIG_WINDOW_MS = 30000;  // 30 segundos para reconfiguração.

// Flag para indicar se uma nova configuração foi recebida.
bool novaConfigRecebida = false;

//////////////////// UUIDS (ALINHADOS COM SEU APP) /////////////
// Define os UUIDs para o serviço e característica BLE.
#define TX_SERVICE_UUID     "0000aaaa-0000-1000-8000-00805f9b34fb"
#define TX_CHAR_UUID        "0000aa01-0000-1000-8000-00805f9b34fb"

// Ponteiro para a característica BLE usada para configuração.
NimBLECharacteristic *configCharacteristic;

//////////////////// FUNÇÕES AUXILIARES ///////////////////////
// Função para converter uma string de MAC em um array de bytes.
bool parseMacString(const String &str, uint8_t mac[6]) {
  int b[6];
  if (sscanf(str.c_str(), "%x:%x:%x:%x:%x:%x",
             &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) == 6) {
    for (int i = 0; i < 6; i++) mac[i] = (uint8_t)b[i];
    return true;
  }
  return false;
}

// Função para analisar uma string de configuração e extrair os valores.
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

// Declaração antecipada de funções auxiliares.
void salvarConfig();
void logConfigSalva();

// Função para processar o payload de configuração recebido via BLE.
// Retorna true se a configuração foi salva com sucesso.
bool processarPayloadConfig(const String &s) {
  if (s.length() == 0) {
    Serial.println("[ERRO] Payload de config vazio.");
    return false;
  }

  String macStr, nome;
  float lat = 0, lon = 0;

  // Tenta analisar a string completa de configuração.
  bool formatoCompleto = parseConfigString(s, macStr, nome, lat, lon);

  uint8_t tempMac[6];
  if (!formatoCompleto) {
    // Caso o formato completo não seja válido, tenta usar apenas o MAC.
    int sep = s.indexOf(';');
    macStr = (sep == -1) ? s : s.substring(0, sep);
    macStr.trim();
    if (!parseMacString(macStr, tempMac)) {
      Serial.println("[ERRO] Formato invalido. Esperado: MAC;NOME;LAT;LON ou apenas MAC.");
      return false;
    }
    Serial.println("[BLE] Payload curto recebido; usando apenas o MAC.");
    nome = "";
    lat = 0;
    lon = 0;
  } else {
    // Caso o formato completo seja válido, analisa os dados recebidos.
    if (!parseMacString(macStr, tempMac)) {
      Serial.println("[ERRO] MAC invalido!");
      return false;
    }

    Serial.println("[BLE] Dados validos recebidos:");
    Serial.println("MAC: " + macStr);
    Serial.println("Nome: " + nome);
    Serial.print("Lat: "); Serial.println(lat);
    Serial.print("Lon: "); Serial.println(lon);
  }

  // Atualiza as variáveis globais com os dados recebidos.
  masterMacStr = macStr;
  sensorName   = nome;
  latitudeCfg  = lat;
  longitudeCfg = lon;
  memcpy(receptorMAC, tempMac, 6);
  salvarConfig();
  logConfigSalva();
  novaConfigRecebida = true;
  return true;
}

#define EEPROM_SIZE 128  // Define o tamanho da EEPROM.

// Função para salvar os dados na EEPROM.
void salvarDadosNaEEPROM(const uint8_t mac[6], const char *nome, float latitude, float longitude) {
    for (int i = 0; i < 6; i++) {
        EEPROM.write(i, mac[i]);
    }
    for (int i = 0; i < 32; i++) {
        EEPROM.write(6 + i, nome[i]);
    }
    EEPROM.put(38, latitude);
    EEPROM.put(42, longitude);
    EEPROM.commit();
    Serial.println("Dados salvos na EEPROM.");
}

// Função para carregar os dados da EEPROM.
bool carregarDadosDaEEPROM(uint8_t mac[6], char *nome, float &latitude, float &longitude) {
    for (int i = 0; i < 6; i++) {
        mac[i] = EEPROM.read(i);
    }
    for (int i = 0; i < 32; i++) {
        nome[i] = EEPROM.read(6 + i);
    }
    EEPROM.get(38, latitude);
    EEPROM.get(42, longitude);

    // Verifica se o MAC é válido (não vazio).
    for (int i = 0; i < 6; i++) {
        if (mac[i] != 0xFF) {
            return true;
        }
    }
    return false;
}

// Função para carregar a configuração salva na EEPROM.
void carregarConfig() {
  Serial.println("Carregando configuração...");

  if (carregarDadosDaEEPROM(receptorMAC, (char*)sensorName.c_str(), latitudeCfg, longitudeCfg)) {
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

// Função para salvar a configuração na EEPROM.
void salvarConfig() {
  Serial.println("Salvando configuração...");

  if (masterMacStr.length() > 0 && parseMacString(masterMacStr, receptorMAC)) {
          salvarDadosNaEEPROM(receptorMAC, sensorName.c_str(), latitudeCfg, longitudeCfg);
          hasConfig = true;
          Serial.println("Config SALVA com sucesso na EEPROM!");
    } else {
        Serial.println("[ERRO] Falha ao salvar configuração. MAC inválido.");
    }
}

//////////////////// LOGS DETALHADOS //////////////////////////
// Função para verificar o estado da memória EEPROM e logar o MAC salvo.
void logEstadoMemoria() {
    Serial.println("[LOG] Verificando estado da memória EEPROM...");
    uint8_t mac[6];
    if (carregarDadosDaEEPROM(mac, (char*)sensorName.c_str(), latitudeCfg, longitudeCfg)) {
        Serial.print("[LOG] MAC encontrado na EEPROM: ");
        for (int i = 0; i < 6; i++) {
            Serial.printf("%02X:", mac[i]);
        }
        Serial.println();
    } else {
        Serial.println("[LOG] Nenhum MAC encontrado na EEPROM.");
    }
}

// Função para logar o pacote recebido via BLE.
void logPacoteRecebido(const String &s) {
    Serial.println("[LOG] Pacote recebido via BLE:");
    Serial.println(s);
}

// Função para logar que a configuração foi salva com sucesso.
void logConfigSalva() {
    Serial.println("[LOG] Configuração salva na EEPROM com sucesso.");
}

//////////////////// LOGS DE CONEXÃO BLE //////////////////////////
// Função para verificar o estado da conexão BLE.
void logBLEConexao() {
    Serial.println("[LOG] Verificando conexão BLE...");
    if (NimBLEDevice::getServer()->getConnectedCount() > 0) {
        Serial.println("[LOG] Dispositivo conectado via BLE.");
    } else {
        Serial.println("[LOG] Nenhum dispositivo conectado via BLE.");
    }
}

//////////////////// CONFIGURAR MTU //////////////////////////
// Função para configurar o MTU (Maximum Transmission Unit) para pacotes BLE.
void configurarMTU() {
    NimBLEDevice::setMTU(256); // Define o MTU para suportar pacotes maiores
    Serial.println("[LOG] MTU configurado para 256 bytes.");
}

//////////////////// CALLBACK BLE ////////////////////////////
class ConfigCallback : public NimBLECharacteristicCallbacks {
public:
  void onWrite(NimBLECharacteristic *pCharacteristic) {
    Serial.println("[LOG] Callback BLE acionado. Verificando pacote recebido...");

    std::string value = pCharacteristic->getValue();
    String s(value.c_str());

    if (s.length() == 0) {
      Serial.println("[ERRO] Pacote recebido está vazio.");
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

//////////////////// CALLBACK DE CONEXÃO BLE //////////////////////////
class ServerCallbacks : public NimBLEServerCallbacks {
public:
  void onConnect(NimBLEServer *pServer) {
    Serial.println("[LOG] Dispositivo conectado ao servidor BLE.");
  }

  void onDisconnect(NimBLEServer *pServer) {
    Serial.println("[LOG] Dispositivo desconectado do servidor BLE.");
    NimBLEDevice::startAdvertising(); // retoma advertising para novo app
  }
};

//////////////////// CALLBACK GLOBAL BLE //////////////////////////
class GlobalServerCallbacks : public NimBLEServerCallbacks {
public:
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
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR, // compatibilidade: write com e sem resposta
    128 // aumenta maxLen para caber string grande
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

    // Log BLE periódico para verificar se houve conexão na janela de configuração
    static unsigned long ultimoLogBLE = 0;
    if (millis() - ultimoLogBLE > 5000) {
      int conexoes = NimBLEDevice::getServer()->getConnectedCount();
      Serial.printf("[LOG] Aguardando config BLE. Conexoes ativas: %d\n", conexoes);
      ultimoLogBLE = millis();
    }

    return;
  }

  // ---------------- MODO RUN -----------------
  float temperatura = dht.readTemperature();
  float umidadeAr   = dht.readHumidity();
  int soloADC       = analogRead(SOIL_PIN);

  int batRaw = analogRead(BAT_PIN);
  float batVoltage = batRaw * (4.2 / 4095.0);

  strcpy(pacote.nome, sensorName.c_str());
  pacote.latitude = latitudeCfg;
  pacote.longitude = longitudeCfg;
  pacote.temperatura = temperatura;
  pacote.umidadeAr   = umidadeAr;
  pacote.soloADC     = soloADC;
  pacote.tensaoBat   = batVoltage;

  esp_now_send(receptorMAC, (uint8_t *)&pacote, sizeof(pacote));

  Serial.printf("Temp: %.2f, Umidade: %.2f, Solo: %d, Bateria: %.2fV, Nome: %s, Lat: %.6f, Lon: %.6f\n",
                temperatura, umidadeAr, soloADC, batVoltage, pacote.nome, pacote.latitude, pacote.longitude);
}


