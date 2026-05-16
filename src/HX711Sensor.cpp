#include "HX711Sensor.h"


HX711Sensor::HX711Sensor(uint8_t dout, uint8_t sck, uint8_t id) : SensorBase(id) {
    _scale.begin(dout, sck);
    _scale.set_scale();
}
    bool HX711Sensor::isValid(){
        return _scale.is_ready();
    }
    void HX711Sensor::readRaw(float & buffer){
        buffer = static_cast<float>(_scale.get_value());
    }