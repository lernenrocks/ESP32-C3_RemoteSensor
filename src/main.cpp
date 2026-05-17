#include <Arduino.h>
#include "SensorManager.h"

char name[64]={"TerraControl SensorBox"};

void setup() {
  Serial.begin(115200);
  SensorManager::initSensors();
  

}

void loop() {
  char json[128];
  SensorManager::getSensorDataJson(json,sizeof(json));
  Serial.println(json);
  delay(1000);
}
