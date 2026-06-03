#include <Arduino.h>
#include "SensorManager.h"
#include "WiFiManager.h"
#include "HttpServer.h"
#include "InternalStorage.h"

extern const char FIRMWARE_VERSION[] = "0.1.0";

void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(PIN_FACTORY_RESET, INPUT_PULLUP);
  if (digitalRead(PIN_FACTORY_RESET) == LOW) {
    InternalStorage::erase();
    ESP.restart();
  }
  SensorManager::initSensors();
}

void loop() {
  if(!WiFiManager::isConnected()){
    WiFiManager::initWifi();
  }
  else {
    HttpServer::handle();
  }
}
