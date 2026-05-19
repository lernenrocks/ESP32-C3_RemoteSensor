#include "DTH22Humidity.h"

DHT22Humidity::DHT22Humidity(DHT *dht){
    _dht=dht;
}

bool DHT22Humidity::isValid(){
    return !isnan(_dht->readTemperature());
}

void DHT22Humidity::readRaw(float &buffer){
    buffer=_dht->readTemperature();
}