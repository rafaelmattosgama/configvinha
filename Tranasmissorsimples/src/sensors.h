#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>

typedef struct {
  float temperatura;
  float umidadeAr;
  int   soloADC;
  float tensaoBat;
  char  nome[32];
  float latitude;
  float longitude;
} DadosEnvio;

void initSensors();
void readAllSensors(DadosEnvio &pacote);

#endif // SENSORS_H
