#include "sensors.h"
#include "app_state.h"
#include <DHT.h>
#include <cstring>

#define DHTPIN 22
#define DHTTYPE DHT11
#define SOIL_PIN 32
#define BAT_PIN 34

static DHT dht(DHTPIN, DHTTYPE);

void initSensors() {
    dht.begin();
    pinMode(SOIL_PIN, INPUT);
    pinMode(BAT_PIN, INPUT);
}

void readAllSensors(DadosEnvio &pacote) {
  float temperatura = dht.readTemperature();
  float umidadeAr   = dht.readHumidity();
  int soloADC       = analogRead(SOIL_PIN);

  int batRaw = analogRead(BAT_PIN);
  float batVoltage = batRaw * (4.2 / 4095.0);

  strncpy(pacote.nome, sensorName.c_str(), sizeof(pacote.nome) - 1);
  pacote.nome[sizeof(pacote.nome) - 1] = '\0';
  pacote.latitude = latitudeCfg;
  pacote.longitude = longitudeCfg;
  pacote.temperatura = temperatura;
  pacote.umidadeAr   = umidadeAr;
  pacote.soloADC     = soloADC;
  pacote.tensaoBat   = batVoltage;
}
