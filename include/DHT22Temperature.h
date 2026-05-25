#pragma once

#include "SensorBase.h"
#include <DHT.h>

class DHT22Temperature : public SensorBase{

    public:
    DHT22Temperature(DHT *dht);
    ~DHT22Temperature()=default;
    bool getCalibrationJson(char *buffer, size_t len) override;
    int getPrecision() override;
    bool calibrate(const JsonObjectConst data) override { return true; }
    bool reset() override { return true; }


    private:
    DHT *_dht;
    bool isValid() override;
    void readRaw(float &buffer) override;

};
