#pragma once
#include <Arduino.h>

namespace InternalStorage
{
    void erase();

    bool begin(const char *pref_namespace, bool readOnly);
    void end();

    bool writeBool(const char *key, const bool value);
    bool writeLong(const char *key, const long value);
    bool writeString(const char *key, const char *value);
    bool writeFloat(const char *key, const float value);

    bool readBool(const char *key, bool &value);
    bool readLong(const char *key, long &value);
    bool readString(const char *key, char *value, size_t len);
    bool readFloat(const char *key, float &value);
}