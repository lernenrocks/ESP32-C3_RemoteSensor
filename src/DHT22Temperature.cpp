#include "DHT22Temperature.h"

DHT22Temperature::DHT22Temperature(DHT *dht){
    _dht=dht;
}

bool DHT22Temperature::isValid(){
    return !isnan(_dht->readTemperature());
}

void DHT22Temperature::readRaw(float &buffer){
    buffer=_dht->readTemperature();
}