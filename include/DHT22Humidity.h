#pragma once

#include "SensorBase.h"
#include <DHT.h>

class DHT22Humidity : public SensorBase{

    public:
    DHT22Humidity(DHT *dht);
    ~DHT22Humidity()=default;
    int getPrecision() override;
    bool getCalibrationJson(char *buffer, size_t len) override;
    bool calibrate(const JsonObjectConst data) override { return true; }
    void reset() override { }


    private:
    DHT *_dht;
    bool isValid() override;
    void readRaw(float &buffer) override;

};
