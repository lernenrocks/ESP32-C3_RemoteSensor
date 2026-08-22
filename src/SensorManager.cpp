#include "SensorManager.h"
#include "SensorBase.h"
#include "HX711Sensor.h"
#include <DHT.h>
#include "DHT22Temperature.h"
#include "DHT22Humidity.h"

DHT dht(DHT22_DATA, DHT22);

namespace
{
    SensorManager::SensorEntry sensorArray[SensorManager::MAX_SENSORS];
    uint8_t sensorCount = 0;

    bool addSensor(SensorBase *sensor, const char *type)
    {
        if (sensorCount < SensorManager::MAX_SENSORS)
        {
            sensorArray[sensorCount++] = {sensor, type};
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
            bool valid = sensorArray[i].sensor->read(value);
            char valueString[16];
            if (valid)
            {
                char formatLiteral[8];
                snprintf(formatLiteral, sizeof(formatLiteral), "%%.%df", sensorArray[i].sensor->getPrecision());
                snprintf(valueString, sizeof(valueString), formatLiteral, value);
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

    bool getCalibrationInfoJson(char buf[], size_t len)
    {
        if (sensorCount == 0)
        {
            snprintf(buf, len, "[]");
            return false;
        }
        bool valid = true;
        size_t offset = 0;
        buf[offset++] = '[';
        for (size_t i = 0; i < sensorCount; i++)
        {
            char steps[256] = {};
            sensorArray[i].sensor->getCalibrationJson(steps, sizeof(steps));
            char currentValues[64] = {};
            sensorArray[i].sensor->getCalibrationValuesJson(currentValues, sizeof(currentValues));
            size_t written = snprintf(buf + offset, len - offset,
                                      "{\"index\":%d,\"type\":\"%s\",\"steps\":%s,\"current\":%s},",
                                      i, sensorArray[i].type, steps, currentValues);
            if (written >= len - offset)
            {
                valid = false;
                break;
            }
            offset += written;
        }
        if (offset < len)
        {
            buf[offset - 1] = ']';
        }
        else
        {
            valid = false;
        }
        if (!valid)
        {
            snprintf(buf, len, "[]");
        }
        return valid;
    }

    void initSensors()
    {
        if (!addSensor(new HX711Sensor(HX711_DOUT, HX711_SCK, sensorCount), SensorType::HX711))
        {
            Serial.println("[ERROR] HX711 sensor not initialized");
        }
        dht.begin();
        if (!addSensor(new DHT22Temperature(&dht), SensorType::DHT22_TEMP))
        {
            Serial.println("[ERROR] DHT22 temperature sensor not initialized");
        }
        if (!addSensor(new DHT22Humidity(&dht), SensorType::DHT22_HUM))
        {
            Serial.println("[ERROR] DHT22 humidity sensor not initialized");
        }
    }

    bool calibrateSensor(uint8_t idx, JsonObjectConst data)
    {
        if (idx >= sensorCount)
        {
            return false;
        }
        else
        {
            return sensorArray[idx].sensor->calibrate(data);
        }
    }

    bool resetSensor(uint8_t idx)
    {
        if (idx >= sensorCount)
        {
            return false;
        }
        sensorArray[idx].sensor->reset();
        return true;
    }
}
