#include "HX711Sensor.h"


HX711Sensor::HX711Sensor(int dout, int sck) {
    _scale.begin(dout, sck);
    _scale.set_scale();
}
    bool HX711Sensor::isValid(){
        return _scale.is_ready();
    }

    bool HX711Sensor::getCalibrationJson(char *buffer, size_t len){
        const char info[] =
            "[{\"instruction\":\"Place empty scale\",\"key\":\"offset\"},"
            "{\"instruction\":\"Add known weight\",\"key\":\"raw_at_weight\",\"ref\":\"ref_weight\"}]";
        size_t written = snprintf(buffer, len, "%s", info);
        return written < len;
    }
    void HX711Sensor::readRaw(float & buffer){
        //! Increase averaging samples if values fluctuate; decrease if MainUnit TCP timeout is hit
        buffer = static_cast<float>(_scale.get_value(3));
    }
    