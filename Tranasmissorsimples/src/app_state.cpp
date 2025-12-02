#include "app_state.h"

String masterMacStr = "";
String sensorName   = "";
float latitudeCfg   = 0;
float longitudeCfg  = 0;
bool hasConfig      = false;

uint8_t receptorMAC[6] = {0};

bool novaConfigRecebida = false;
Modo modoAtual = MODO_CONFIG;
