#pragma once

#include "SensorBase.h"
#include <DHT.h>

class DHT22Humidity : public SensorBase{

    public:
    DHT22Humidity(DHT *dht);
    ~DHT22Humidity()=default;

    private:
    DHT *_dht;
    bool isValid() override;
    void readRaw(float &buffer) override;

};