#include "System.h"
#include "InternalStorage.h"
#include "DigestCrypto.h"
#include "initialPW.h"

namespace
{
    const char *NAME_KEY = "name";
    const char *HA1_KEY = "ha1";
    const char *PASSWORD_KEY = "password";

    void nvsBegin(bool readOnly)
    {
        InternalStorage::begin("System", readOnly);
    }
    void nvsEnd()
    {
        InternalStorage::end();
    }
    void loadDeviceHa1(char *buffer, size_t len)
    {
        nvsBegin(true);
        InternalStorage::readString(HA1_KEY, buffer, len);
        nvsEnd();
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
        nvsBegin(true);
        InternalStorage::readString(NAME_KEY, buffer, len);
        nvsEnd();
    }

    void storeDeviceName(const char *name)
    {
        nvsBegin(false);
        InternalStorage::writeString(NAME_KEY, name);
        nvsEnd();
    }

    void provideDeviceDefaultPassword(char *buffer, size_t len)
    {
        strlcpy(buffer, initialPW, len);
    }

    void loadDevicePassword(char *buffer, size_t len)
    {
        nvsBegin(true);
        InternalStorage::readString(PASSWORD_KEY, buffer, len);
        nvsEnd();
    }

    void storeDevicePassword(const char *password)
    {
        nvsBegin(false);
        InternalStorage::writeString(PASSWORD_KEY, password);
        nvsEnd();
    }

    void storeDeviceHa1(const char *ha1)
    {
        nvsBegin(false);
        InternalStorage::writeString(HA1_KEY, ha1);
        nvsEnd();
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