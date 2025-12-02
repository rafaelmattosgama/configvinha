#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

bool initConfigStorage();
bool carregarConfig();
void salvarConfig();
void logEstadoMemoria();
void logPacoteRecebido(const String &s);
bool processarPayloadConfig(const String &s);

bool parseMacString(const String &str, uint8_t mac[6]);
bool parseConfigString(const String &s, String &macStr, String &nome, float &lat, float &lon);

#endif // CONFIG_H
