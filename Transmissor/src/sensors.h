#pragma once

struct SensorReadings {
  float temperatura = 0.0f;
  float umidadeAr = 0.0f;
  int soloADC = 0;
  float tensaoBat = 0.0f;
};

void initSensors();
SensorReadings readSensors();

