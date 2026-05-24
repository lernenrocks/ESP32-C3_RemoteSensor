#pragma once

#include "SensorBase.h"
#include <DHT.h>

class DHT22Humidity : public SensorBase{

    public:
    DHT22Humidity(DHT *dht);
    ~DHT22Humidity()=default;
    int getPrecision() override;
    bool getCalibrationJson(char *buffer, size_t len) override;
    virtual bool calibrate(JsonObjectConst data) { return true; }
    bool reset() override { return true; }


    private:
    DHT *_dht;
    bool isValid() override;
    void readRaw(float &buffer) override;

};