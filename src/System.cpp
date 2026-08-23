#include "System.h"
#include "InternalStorage.h"
#include "DigestCrypto.h"
#include "initialPW.h"
#include <WiFi.h>

namespace
{
    const char *NAME_KEY = "name";
    const char *HA1_KEY = "ha1";
    const char *PASSWORD_KEY = "password";
    const char *SYSTEM_NAMESPACE = "System";

    void loadDeviceHa1(char *buffer, size_t len)
    {
        InternalStorage::Session session(SYSTEM_NAMESPACE, true);
        InternalStorage::readString(HA1_KEY, buffer, len);
    }
    void provideDeviceDefaultHa1(char *buffer, size_t len)
    {
        DigestCrypto::computeHa1(initialPW, buffer, len);
    }

}
namespace System
{

    void loadDeviceName(char *buffer, size_t len)
    {
        InternalStorage::Session session(SYSTEM_NAMESPACE, true);
        InternalStorage::readString(NAME_KEY, buffer, len);
    }

    void storeDeviceName(const char *name)
    {
        InternalStorage::Session session(SYSTEM_NAMESPACE, false);
        InternalStorage::writeString(NAME_KEY, name);
    }

    bool getActiveDeviceName(char *buffer, size_t len)
    {
        loadDeviceName(buffer, len);
        if (buffer[0] == '\0')
        {
            uint8_t mac[6];
            WiFi.macAddress(mac);
            snprintf(buffer, len, "SensorNode-%02X%02X%02X%02X%02X%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            return false;
        }
        return true;
    }

    void provideDeviceDefaultPassword(char *buffer, size_t len)
    {
        strlcpy(buffer, initialPW, len);
    }

    void loadDevicePassword(char *buffer, size_t len)
    {
        InternalStorage::Session session(SYSTEM_NAMESPACE, true);
        InternalStorage::readString(PASSWORD_KEY, buffer, len);
    }

    void storeDevicePassword(const char *password)
    {
        InternalStorage::Session session(SYSTEM_NAMESPACE, false);
        InternalStorage::writeString(PASSWORD_KEY, password);
    }

    void storeDeviceHa1(const char *ha1)
    {
        InternalStorage::Session session(SYSTEM_NAMESPACE, false);
        InternalStorage::writeString(HA1_KEY, ha1);
    }
    bool getActiveHa1(char *buffer, size_t len)
    {
        loadDeviceHa1(buffer, len);
        if (buffer[0] == '\0')
        {
            provideDeviceDefaultHa1(buffer, len);
            return false;
        }
        return true;
    }

    bool getActivePassword(char *buffer, size_t len)
    {
        loadDevicePassword(buffer, len);
        if (buffer[0] == '\0')
        {
            provideDeviceDefaultPassword(buffer, len);
            return false;
        }
        return true;
    }
}