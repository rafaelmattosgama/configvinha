#include "sensors.h"

#include <Arduino.h>
#include <DHT.h>

#include "config_defs.h"

static DHT dht(DHTPIN, DHTTYPE);

void initSensors() {
  dht.begin();
  pinMode(SOIL_PIN, INPUT);
  pinMode(BAT_PIN, INPUT);
}

SensorReadings readSensors() {
  SensorReadings r;
  r.temperatura = dht.readTemperature();
  r.umidadeAr = dht.readHumidity();
  r.soloADC = analogRead(SOIL_PIN);

  int batRaw = analogRead(BAT_PIN);
  r.tensaoBat = batRaw * (4.2f / 4095.0f);
  return r;
}

