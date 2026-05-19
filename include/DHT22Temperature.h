#pragma once

#include "SensorBase.h"
#include <DHT.h>

class DHT22Temperature : public SensorBase{

    public:
    DHT22Temperature(DHT *dht);
    ~DHT22Temperature()=default;

    private:
    DHT *_dht;
    bool isValid() override;
    void readRaw(float &buffer) override;

};