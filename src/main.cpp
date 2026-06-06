#include <Arduino.h>
#include "SensorManager.h"
#include "WiFiManager.h"
#include "HttpServer.h"
#include "InternalStorage.h"
#include "esp_wifi.h"
#include "esp_sleep.h"

extern const char FIRMWARE_VERSION[] = "0.1.0";
constexpr unsigned long FACTORY_RESET_TRESHOLD = 5000UL;

// Zaehlt abgeschlossene Light-Sleep/Wake-Zyklen. Wird in GET /status
// ausgegeben — macht den Sleep ueber WiFi pruefbar (die USB-Konsole stirbt
// im Sleep, daher kein Serial-Weg). HttpServer liest es via extern.
uint32_t wakeCount = 0;

// Wach-Fenster nach einem WiFi-Wakeup: so lange auf eingehende Requests
// warten, bevor wieder geschlafen wird. Deckt den TCP-Handshake ab, der
// nach dem Aufwachen noch laeuft.
constexpr unsigned long HTTP_GRACE_MS = 200UL;

void checkFactoryReset()
{
  static bool pressed = false;
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
      digitalWrite(PIN_INTERNAL_LED, HIGH);
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
    if (pressingTime < FACTORY_RESET_TRESHOLD)
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
  digitalWrite(PIN_INTERNAL_LED, HIGH);
  SensorManager::initSensors();

  // Wakeup-Quellen: eingehende TCP-Anfrage (WiFi, braucht WIFI_PS_MIN_MODEM,
  // gesetzt in WiFiManager) und die Factory-Reset-Taste (GPIO9). Kein Timer,
  // kein UART — der C3 schlaeft, bis ein Request oder die Taste ihn weckt.
  gpio_wakeup_enable((gpio_num_t)PIN_FACTORY_RESET, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  esp_sleep_enable_wifi_wakeup();
}

void loop()
{
  checkFactoryReset();

  if (!WiFiManager::isConnected())
  {
    // AP-/Provisioning-Modus oder STA-Reconnect: kein Sleep, Requests aber
    // weiter bedienen (erste Config laeuft ueber den SoftAP).
    WiFiManager::initWifi();
    HttpServer::handle();
    return;
  }

#ifdef DISABLE_LIGHT_SLEEP
  // Debug-Build: kein Light Sleep. Der USB-Serial-JTAG-Controller bleibt an,
  // Konsole und Upload sind durchgehend stabil. Requests normal bedienen.
  HttpServer::handle();
  return;
#else
  Serial.flush();
  esp_err_t err = esp_light_sleep_start();

  // Nur ein erfolgreicher Schlaf zaehlt als Wake-Zyklus (Reject != geschlafen).
  if (err == ESP_OK)
  {
    ++wakeCount;
    // Kein Netzwerk-Wakeup -> nichts zu tun, weiter.
    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_WIFI)
      return;
  }

  // Sonst gilt: entweder hat uns WiFi geweckt, ODER der Schlafversuch wurde
  // abgelehnt (ESP_ERR_SLEEP_REJECT == 259 — ein Wakeup-Trigger lag schon an,
  // meist pending WiFi-RX). Beides heisst: es koennte ein Request anliegen.
  // Der TCP-Handshake laeuft nach dem Aufwachen noch, ein einzelnes handle()
  // wuerde ihn verpassen — also ein Wach-Fenster offen halten und bei jedem
  // bedienten Request neu aufziehen. Das drainiert zugleich die pending
  // Arbeit, sodass die naechste Iteration sauber einschlaeft.
  unsigned long idleSince = millis();
  while (millis() - idleSince < HTTP_GRACE_MS)
  {
    if (HttpServer::handle())
      idleSince = millis();
    else
      delay(1); // yield: WiFi-Task atmen lassen, kein Busy-Spin
  }
#endif
}
