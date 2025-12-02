#ifndef APP_STATE_H
#define APP_STATE_H

#include <Arduino.h>

// Estados de operacao
enum Modo { MODO_CONFIG, MODO_RUN };

// Variaveis globais de configuracao
extern String masterMacStr;
extern String sensorName;
extern float latitudeCfg;
extern float longitudeCfg;
extern bool hasConfig;

// MAC do receptor em formato binario
extern uint8_t receptorMAC[6];

// Flags de estado
extern bool novaConfigRecebida;
extern Modo modoAtual;

#endif // APP_STATE_H
