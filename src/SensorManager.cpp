#include "SensorManager.h"
#include "SensorBase.h"
#ifdef SENSOR_HX711
#include "HX711Sensor.h"
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
#ifdef SENSOR_HX711
        if (!addSensor(new HX711Sensor(HX711_DOUT, HX711_SCK)))
        {
            Serial.println("Sensor nicht initialisiert");
        }
#endif
    }
}
