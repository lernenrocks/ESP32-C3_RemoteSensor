#include <Arduino.h>
#include "SensorManager.h"
#include "WiFiManager.h"
#include "HttpServer.h"
#include "InternalStorage.h"
#include "esp_sleep.h"

extern const char FIRMWARE_VERSION[] = "1.0.0";
constexpr unsigned long FACTORY_RESET_TRESHOLD = 5000UL;

// Counts completed Light Sleep/wake cycles. Output in GET /status — makes
// sleep verifiable over WiFi (the USB console dies during sleep, so no
// Serial path). HttpServer reads it via extern.
uint32_t wakeCount = 0;

// Wake window after a WiFi wakeup: how long to wait for incoming requests
// before sleeping again. Covers the TCP handshake that's still running
// after waking up.
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
#ifdef DISABLE_LIGHT_SLEEP
      Serial.println("[WARN] Factory Reset triggered via GPIO9, erasing NVS...");
#endif
      InternalStorage::erase();
      ESP.restart();
    }
  }
  if (pressed)
  {
    // Two blink rates as feedback for how much longer to hold: slow (500ms)
    // while still counting down, fast (200ms) once the threshold is reached
    // — fast blinking signals "release now to trigger the reset".
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
  delay(5000);
  pinMode(PIN_FACTORY_RESET, INPUT_PULLUP);
  pinMode(PIN_INTERNAL_LED, OUTPUT);
  digitalWrite(PIN_INTERNAL_LED, HIGH);
  SensorManager::initSensors();

  // Wakeup sources: incoming TCP request (WiFi, needs WIFI_PS_MIN_MODEM, set
  // in WiFiManager) and the factory reset button (GPIO9). No timer, no
  // UART — the C3 sleeps until a request or the button wakes it.
  gpio_wakeup_enable((gpio_num_t)PIN_FACTORY_RESET, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  esp_sleep_enable_wifi_wakeup();
}

void loop()
{
  checkFactoryReset();

#ifdef DISABLE_LIGHT_SLEEP
  WiFiManager::heartbeat(); // debug-only status output; silent in the sleep build
#endif

  if (!WiFiManager::isConnected())
  {
    // AP/provisioning mode or STA reconnect: no sleep, but keep serving
    // requests (initial config runs over the SoftAP).
    WiFiManager::initWifi();
    HttpServer::handle();
    return;
  }

#ifdef DISABLE_LIGHT_SLEEP
  // Debug build: no Light Sleep. The USB-Serial-JTAG controller stays on,
  // console and upload remain stable throughout. Serve requests normally.
  HttpServer::handle();
  return;
#else
  Serial.flush();
  esp_err_t err = esp_light_sleep_start();

  // Only a successful sleep counts as a wake cycle (reject != slept).
  if (err == ESP_OK)
  {
    ++wakeCount;
    // No network wakeup -> nothing to do, continue.
    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_WIFI)
      return;
  }

  // Otherwise: either WiFi woke us, OR the sleep attempt was rejected
  // (ESP_ERR_SLEEP_REJECT == 259 — a wakeup trigger was already pending,
  // usually pending WiFi RX). Either way means a request could be waiting.
  // The TCP handshake is still running after waking up, a single handle()
  // call would miss it — so keep a wake window open and reset it on every
  // serviced request. This also drains the pending work, so the next
  // iteration falls asleep cleanly.
  unsigned long idleSince = millis();
  while (millis() - idleSince < HTTP_GRACE_MS)
  {
    if (HttpServer::handle())
      idleSince = millis();
    else
      delay(1); // yield: let the WiFi task breathe, no busy-spin
  }
#endif
}
