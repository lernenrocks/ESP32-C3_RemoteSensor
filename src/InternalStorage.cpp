#include "InternalStorage.h"
#include <Arduino.h>
#include <Preferences.h>
#include "nvs_flash.h"

Preferences preferences;

namespace InternalStorage
{
    void erase(){
        nvs_flash_erase();
        nvs_flash_init();

    }

    bool begin(const char *pref_namespace, bool readOnly){
        return preferences.begin(pref_namespace,readOnly);
    }
    void end(){
        preferences.end();
    }

    bool writeBool(const char *key, const bool value){
       return preferences.putBool(key,value) > 0; //returns bytes written
    }
    bool writeLong(const char *key, const long value){
        return preferences.putLong(key,value) > 0;
    }
    bool writeString(const char *key, const char *value){
        return preferences.putString(key,value) > 0;
    }
    bool writeFloat(const char *key, const float value){
        return preferences.putFloat(key,value) > 0;
    }

    bool readBool(const char *key, bool &value){
        if(!preferences.isKey(key)){
            return false;
        }
        value = preferences.getBool(key);
        return true;
    }
    bool readLong(const char *key, long &value){
        if(!preferences.isKey(key)){
            return false;
        }
        value = preferences.getLong(key);
        return true;
    }
    bool readString(const char *key, char *value, size_t len){
        if(!preferences.isKey(key)){
            // Guarantee a valid, empty string on every path instead of relying
            // on the caller having zero-initialized `value` beforehand — the
            // "existence check via first byte" convention used throughout
            // this project (see CLAUDE.md) then holds even if a future
            // caller forgets to pre-zero its buffer.
            if(len > 0) value[0] = '\0';
            return false;
        }
        preferences.getString(key,value,len);
        return true;
    }
    bool readFloat(const char *key, float &value){
        if(!preferences.isKey(key)){
            return false;
        }
        value = preferences.getFloat(key);
        return true;
    }
}