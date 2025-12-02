#include "config.h"
#include "app_state.h"
#include <Preferences.h>
#include <string.h>

Preferences prefs;
static const char *PREF_NAMESPACE = "txcfg";
static const char *KEY_MAC  = "mac";
static const char *KEY_NAME = "name";
static const char *KEY_LAT  = "lat";
static const char *KEY_LON  = "lon";

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

bool initConfigStorage() {
  if (!prefs.begin(PREF_NAMESPACE, false)) {
    Serial.println("[ERRO] Falha ao iniciar Preferences.");
    return false;
  }
  return true;
}

static bool carregarPreferences() {
  masterMacStr = prefs.getString(KEY_MAC, "");
  sensorName   = prefs.getString(KEY_NAME, "");
  latitudeCfg  = prefs.getFloat(KEY_LAT, 0);
  longitudeCfg = prefs.getFloat(KEY_LON, 0);

  if (masterMacStr.length() == 0) return false;
  if (!parseMacString(masterMacStr, receptorMAC)) return false;
  return true;
}

static void salvarPreferences() {
  prefs.putString(KEY_MAC, masterMacStr);
  prefs.putString(KEY_NAME, sensorName);
  prefs.putFloat(KEY_LAT, latitudeCfg);
  prefs.putFloat(KEY_LON, longitudeCfg);
}

void logPacoteRecebido(const String &s) {
  Serial.println("[LOG] Pacote recebido via BLE:");
  Serial.println(s);
}

bool processarPayloadConfig(const String &s) {
  if (s.length() == 0) {
    Serial.println("[ERRO] Payload de config vazio.");
    return false;
  }

  String macStr, nome;
  float lat = 0, lon = 0;

  bool formatoCompleto = parseConfigString(s, macStr, nome, lat, lon);

  uint8_t tempMac[6];
  if (!formatoCompleto) {
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

bool carregarConfig() {
  Serial.println("Carregando configuracao...");

  if (carregarPreferences()) {
      hasConfig = true;
      Serial.println("Config encontrada nas Preferences:");
      Serial.print("Master MAC: ");
      for (int i = 0; i < 6; i++) {
          Serial.printf("%02X:", receptorMAC[i]);
      }
      Serial.println();
      if (sensorName.length() > 0) Serial.println("Nome: " + sensorName);
      Serial.print("Lat: "); Serial.println(latitudeCfg);
      Serial.print("Lon: "); Serial.println(longitudeCfg);
  } else {
      Serial.println("Nenhuma configuracao existente nas Preferences.");
      hasConfig = false;
  }
  return hasConfig;
}

void salvarConfig() {
  Serial.println("Salvando configuracao...");

  if (masterMacStr.length() > 0 && parseMacString(masterMacStr, receptorMAC)) {
          salvarPreferences();
          hasConfig = true;
          Serial.println("Config SALVA com sucesso nas Preferences!");
    } else {
        Serial.println("[ERRO] Falha ao salvar configuracao. MAC invalido.");
    }
}

void logEstadoMemoria() {
    Serial.println("[LOG] Verificando estado da memoria (Preferences)...");
    String macStr = prefs.getString(KEY_MAC, "");
    if (macStr.length() == 0) {
        Serial.println("[LOG] Nenhum MAC encontrado nas Preferences.");
        return;
    }
    Serial.print("[LOG] MAC encontrado nas Preferences: ");
    Serial.println(macStr);
    String nome = prefs.getString(KEY_NAME, "");
    if (nome.length() > 0) Serial.println("[LOG] Nome salvo: " + nome);
    Serial.print("[LOG] Lat/Lon salvos: ");
    Serial.print(prefs.getFloat(KEY_LAT, 0));
    Serial.print(" / ");
    Serial.println(prefs.getFloat(KEY_LON, 0));
}

void logConfigSalva() {
    Serial.println("[LOG] Configuracao salva nas Preferences com sucesso.");
}
