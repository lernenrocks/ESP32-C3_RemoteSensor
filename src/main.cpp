#include <Arduino.h>
#include "SensorManager.h"
#include "WiFiManager.h"

char name[64]={"TerraControl SensorBox"};

void setup() {
  Serial.begin(115200);
  SensorManager::initSensors();
}

void loop() {
  if(!WiFiManager::isConnected()){
    WiFiManager::initWifi();
  }
  delay(1000);
}
