#include "HX711Sensor.h"

HX711Sensor::HX711Sensor(int dout, int sck)
{
    _scale.begin(dout, sck);
    _scale.set_scale();
}
bool HX711Sensor::isValid()
{
    return _scale.is_ready();
}

bool HX711Sensor::getCalibrationJson(char *buffer, size_t len)
{
    const char info[] =
        "[{\"instruction\":\"Remove all weight, then confirm\",\"key\":\"offset\"},"
        "{\"instruction\":\"Add known weight\",\"key\":\"ref_weight\",\"ref\":\"ref_weight\"}]";
    size_t written = snprintf(buffer, len, "%s", info);
    return written < len;
}
void HX711Sensor::readRaw(float &buffer)
{
    //! Increase averaging samples if values fluctuate; decrease if MainUnit TCP timeout is hit
    buffer = static_cast<float>(_scale.get_units(5));
}
int HX711Sensor::getPrecision()
{
    return 0;
}
bool HX711Sensor::calibrate(const JsonObjectConst data){
    if(data["offset"].isNull() || data["ref_weight"].isNull()){
        return false;
    }
    long offset = data["offset"];
    float refWeight = data["ref_weight"];
    if(refWeight<=0){
        return false;
    }
    HX711Sensor::setOffset(offset);
    HX711Sensor::setScale(refWeight);
    return true;
}

void HX711Sensor::setOffset(long offset){
    _scale.set_offset(offset);

}
void HX711Sensor::setScale(float referenceWeightInGramm)
{
    float value=_scale.get_value(5);
    _scale.set_scale(value/referenceWeightInGramm);
}

bool HX711Sensor::reset()
{
    _scale.set_scale(1.0f);
    _scale.set_offset(0);
    return true;
}