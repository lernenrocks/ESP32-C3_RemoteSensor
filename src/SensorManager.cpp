#include "SensorManager.h"
#include "SensorBase.h"
#include "HX711Sensor.h"
#include <DHT.h>
#include "DHT22Temperature.h"
#include "DHT22Humidity.h"
#include "InternalStorage.h"
#include "JsonEscape.h"

DHT dht(DHT22_DATA, DHT22);
namespace SensorType
{
    constexpr const char *HX711 = "HX711";
    constexpr const char *DHT22_TEMP = "DHT22_TEMP";
    constexpr const char *DHT22_HUM = "DHT22_HUMIDITY";
}
namespace
{
    constexpr uint8_t MAX_SENSORS = 4;
    constexpr uint8_t KEY_LEN = 15;
    constexpr const char *NVS_NAMESPACE_SENSOR_META = "SensorMeta";
    constexpr const char *SENSOR_NAME_KEY_PREFIX = "sName";
    constexpr const char *VALUE_OFFSET_KEY_PREFIX = "vOffset";
    constexpr const char *SENSOR_NAME_PREFIX = "Sensor";
    struct SensorEntry
    {
        SensorBase *sensor;
        const char *type;
        float valueOffset;
    };

    SensorEntry sensorArray[MAX_SENSORS];
    uint8_t sensorCount = 0;

    bool addSensor(SensorBase *sensor, const char *type)
    {
        if (sensorCount < MAX_SENSORS)
        {
            float valueOffset = 0.0f;
            char key[KEY_LEN] = {};
            snprintf(key, sizeof(key), "%s%d", VALUE_OFFSET_KEY_PREFIX, sensorCount);
            InternalStorage::Session session(NVS_NAMESPACE_SENSOR_META, true);
            InternalStorage::readFloat(key, valueOffset);
            sensorArray[sensorCount] = {sensor, type, valueOffset};
            sensorCount++;
            return true;
        }
        return false;
    }
}

namespace SensorManager
{

    bool getSensorName(uint8_t idx, char *buffer, size_t len)
    {
        if (idx >= sensorCount)
            return false;
        char key[KEY_LEN] = {};
        snprintf(key, sizeof(key), "%s%d", SENSOR_NAME_KEY_PREFIX, idx);
        InternalStorage::Session session(NVS_NAMESPACE_SENSOR_META, true);
        if (!InternalStorage::readString(key, buffer, len) || buffer[0] == '\0')
        {
            snprintf(buffer, len, "%s %d", SENSOR_NAME_PREFIX, idx);
        }
        return true;
    }

    bool setSensorName(uint8_t idx, const char *name)
    {
        if (idx >= sensorCount || strlen(name) >= SENSOR_NAME_LEN)
        {
            return false;
        }
        char key[KEY_LEN] = {};
        snprintf(key, sizeof(key), "%s%d", SENSOR_NAME_KEY_PREFIX, idx);
        InternalStorage::Session session(NVS_NAMESPACE_SENSOR_META, false);
        return InternalStorage::writeString(key, name);
    }

    bool setValueOffset(uint8_t idx, float value)
    {
        if (idx >= sensorCount)
            return false;
        char key[KEY_LEN] = {};
        snprintf(key, sizeof(key), "%s%d", VALUE_OFFSET_KEY_PREFIX, idx);
        InternalStorage::Session session(NVS_NAMESPACE_SENSOR_META, false);
        if (InternalStorage::writeFloat(key, value))
        {
            sensorArray[idx].valueOffset = value;
            return true;
        }
        return false;
    }

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
                value-=sensorArray[i].valueOffset; //substracts offset (aka tara).
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
            if (!sensorArray[i].sensor->getCalibrationJson(steps, sizeof(steps)))
            {
                snprintf(steps, sizeof(steps), "[]"); // truncated -> fall back to a valid, empty value
            }
            char currentValues[64] = {};
            if (!sensorArray[i].sensor->getCalibrationValuesJson(currentValues, sizeof(currentValues)))
            {
                snprintf(currentValues, sizeof(currentValues), "{}");
            }
            char name[SENSOR_NAME_LEN]={};
            getSensorName(i,name,sizeof(name));
            char nameEscaped[SENSOR_NAME_LEN*6]={};
            JsonEscape::escape(name,nameEscaped,sizeof(nameEscaped));
            size_t written = snprintf(buf + offset, len - offset,
                                      "{\"index\":%d,\"name\":\"%s\",\"type\":\"%s\",\"unit\":\"%s\",\"offset\":%f,\"steps\":%s,\"current\":%s},",
                                      i, nameEscaped, sensorArray[i].type, sensorArray[i].sensor->getUnit(),sensorArray[i].valueOffset, steps, currentValues);
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

            if (sensorArray[idx].sensor->calibrate(data))
            {
                setValueOffset(idx,0.0f);
                return true;
            }
            return false;
        }
    }

    bool resetSensor(uint8_t idx)
    {
        if (idx >= sensorCount)
        {
            return false;
        }
        setValueOffset(idx,0.0f);
        sensorArray[idx].sensor->reset();
        return true;
    }
}
