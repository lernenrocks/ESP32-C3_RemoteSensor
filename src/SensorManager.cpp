#include "SensorManager.h"
#include "SensorBase.h"
#ifdef HX711_DHT22
#include "HX711Sensor.h"
#include <DHT.h>
#include "DHT22Temperature.h"
#include "DTH22Humidity.h"
DHT dht(DHT22_DATA,DHT22);
#endif

namespace
{
    SensorBase *sensorArray[SensorManager::MAX_SENSORS];
    uint8_t sensorCount = 0;

    bool addSensor(SensorBase *sensor)
    {
        if (sensorCount < SensorManager::MAX_SENSORS)
        {
            sensorArray[sensorCount++] = sensor;
            return true;
        }
        return false;
    }
}

namespace SensorManager
{

    bool getSensorDataJson(char sensorDataJson[], size_t len)
    {
        if (sensorCount == 0)
        {
            snprintf(sensorDataJson, len, "{}");
            return false;
        }
        bool JsonValid = true;
        size_t offset = 0;
        sensorDataJson[offset++] = '{';
        for (size_t i = 0; i < sensorCount; i++)
        {
            float value = 0.0f;
            bool valid = sensorArray[i]->read(value);
            char valueString[16];
            if (valid)
            {
                snprintf(valueString, sizeof(valueString), "%.0f", value);
            }
            else
            {
                snprintf(valueString, sizeof(valueString), "null");
            }
            size_t written = snprintf(sensorDataJson + offset, len - offset, "\"sensor:%d\": { \"value\": %s, \"valid\":%s },", i, valueString, valid ? "true" : "false");
            if (written >= len - offset)
            {
                JsonValid = false;
                break;
            }
            offset += written;
        }

        if (offset < len)
        {
            sensorDataJson[offset - 1] = '}'; // set } instead of last comma
        }
        else
        {
            JsonValid = false;
        }
        if (!JsonValid)
        {
            snprintf(sensorDataJson, len, "{}");
        }
        return JsonValid;
    }

    void initSensors()
    {
#ifdef HX711_DHT22
        if (!addSensor(new HX711Sensor(HX711_DOUT, HX711_SCK)))
        {
            Serial.println("HX11 Sensor nicht initialisiert");
        }
        dht.begin();
        if(!addSensor(new DHT22Temperature(&dht))){
            Serial.println("DHT22 Temperatursensor nicht initialisiert");
        }
                if(!addSensor(new DHT22Humidity(&dht))){
            Serial.println("DHT22 Temperatursensor nicht initialisiert");
        }
#endif
    }
}
