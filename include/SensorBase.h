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

    private:
    virtual bool isValid()=0;
    virtual void readRaw(float & buffer)=0;
};
