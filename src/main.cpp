#include <Arduino.h>
#include "SensorManager.h"
#include "WiFiManager.h"
#include "HttpServer.h"


extern const char FIRMWARE_VERSION[] = "0.1.0";

void setup() {
  Serial.begin(115200);
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
