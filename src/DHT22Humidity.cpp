#include "DHT22Humidity.h"

DHT22Humidity::DHT22Humidity(DHT *dht) : _dht(dht){}

bool DHT22Humidity::isValid(){
    return !isnan(_dht->readHumidity());
}

bool DHT22Humidity::getCalibrationJson(char *buffer, size_t len){
    size_t written = snprintf(buffer,len,"[]");
    return written <len;
}

bool DHT22Humidity::getCalibrationValuesJson(char *buffer, size_t len){
    size_t written = snprintf(buffer,len,"{}");
    return written <len;
}

void DHT22Humidity::readRaw(float &buffer){
    buffer=_dht->readHumidity();
}
int DHT22Humidity::getPrecision(){
    return 1;
}
const char* DHT22Humidity::getUnit(){
    return "%";
}