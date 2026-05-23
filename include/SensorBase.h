#pragma once
#include <Arduino.h>
/**
 * @brief Abstract base class for all sensors
 */
class SensorBase
{
public:
SensorBase(){
}
virtual ~SensorBase() = default;
    /**
     * @brief reads the value of a sensor
     * @param value buffer for sensor value
     * @return true, if value is valid
     */
    bool read(float &value){
        if(!isValid()) return false;
        readRaw(value);
        return true;
        }
    /**
     * @brief getter for calibration info
     * @param buffer buffer for info
     * @param len length of the buffer
     * @return buffer is valid
     */
    virtual bool getCalibrationJson(char *buffer, size_t len)=0;

    /**
     * get specific precision of the sensor value
     * @return precision after comma
     */
    virtual int getPrecision()=0;

    private:
    virtual bool isValid()=0;
    virtual void readRaw(float & buffer)=0;
};
