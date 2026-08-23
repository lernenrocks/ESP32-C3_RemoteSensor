#pragma once
#include "HX711.h"
#include "SensorBase.h"

class HX711Sensor : public SensorBase
{
public:
    HX711Sensor(int dout, int sck, uint8_t pId);
    ~HX711Sensor() = default;
    bool getCalibrationJson(char *buffer, size_t len) override;
    bool getCalibrationValuesJson(char *buffer, size_t len) override;
    int getPrecision() override;
    const char* getUnit() override;
    bool calibrate(const JsonObjectConst data) override;
    void reset() override;

private:
    static constexpr size_t NVS_NAMESPACE_LEN = 12;
    HX711 _scale;
    uint8_t _pId;
    // Builds this instance's own NVS namespace ("sensor_<pId>") on demand.
    // _pId is stored per instance (not a shared/global buffer), so this
    // stays correct no matter how many other HX711Sensor instances exist.
    void nvsNamespace(char *out, size_t len) const;
    bool isValid() override;
    void readRaw(float &buffer) override;
};
