#ifndef ESPNOW_H
#define ESPNOW_H

#include "sensors.h"

bool startEspNow();
void sendEspNow(const DadosEnvio &pacote);

#endif // ESPNOW_H
