#pragma once
#include "HX711.h"
#include "SensorBase.h"

class HX711Sensor : public SensorBase
{
public:
    HX711Sensor(int dout, int sck);
    ~HX711Sensor() = default;
    bool getCalibrationJson(char *buffer, size_t len) override;
    int getPrecision() override;
    bool calibrate(const JsonObjectConst data) override;
    bool reset() override;

private:
    HX711 _scale;
    bool isValid() override;
    void readRaw(float &buffer) override;
    /**
     * @brief first calibration method. Trigger on empty scale only. Must been called before setScale
     */
    void setOffset(long offset);
    /**
     * @brief second calibration method. Must been called after setOffset
     */
    void setScale(float referenceWeightInGramm);
};