#include <Arduino.h>
#include "SensorManager.h"
#include "WiFiManager.h"
#include "HttpServer.h"
#include "InternalStorage.h"

extern const char FIRMWARE_VERSION[] = "0.1.0";
constexpr unsigned long FACTORY_RESET_TRESHOLD = 5000UL;

void checkFactoryReset()
{
  static bool pressed = false; // true if pressed
  static unsigned long pressedStarted = 0;
  if (digitalRead(PIN_FACTORY_RESET) == LOW && !pressed)
  {
    delay(50);
    pressedStarted = millis();
    pressed = true;
  }
  else if (digitalRead(PIN_FACTORY_RESET) == HIGH && pressed)
  {
    delay(50);
    if (millis() - pressedStarted < FACTORY_RESET_TRESHOLD)
    {
      pressedStarted = 0;
      pressed = false;
      digitalWrite(PIN_INTERNAL_LED,HIGH);
    }
    else
    {
      Serial.println("Factory Reset");
      InternalStorage::erase();
      ESP.restart();
    }
  }
  if (pressed)
  {
    unsigned long pressingTime = millis() - pressedStarted;
    static unsigned long lastBlink = millis();
    if (pressingTime < 5000)
    {
      if (millis() - lastBlink > 500)
      {
        digitalWrite(PIN_INTERNAL_LED, !digitalRead(PIN_INTERNAL_LED));
        lastBlink = millis();
      }
    }
    else
    {
      if (millis() - lastBlink > 200)
      {
        digitalWrite(PIN_INTERNAL_LED, !digitalRead(PIN_INTERNAL_LED));
        lastBlink = millis();
      }
    }
  }
  
}

void setup()
{
  Serial.begin(115200);
  delay(1000);
  pinMode(PIN_FACTORY_RESET, INPUT_PULLUP);
  pinMode(PIN_INTERNAL_LED, OUTPUT);
  digitalWrite(PIN_INTERNAL_LED,HIGH);
  SensorManager::initSensors();
}

void loop()
{
  checkFactoryReset();
  if (!WiFiManager::isConnected())
  {
    WiFiManager::initWifi();
  }
  HttpServer::handle();
}
