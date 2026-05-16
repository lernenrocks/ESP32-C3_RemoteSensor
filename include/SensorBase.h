#pragma once
#include <Arduino.h>
/**
 * @brief Abstract base class for all sensors
 */
class SensorBase
{
public:
SensorBase(size_t p_id):_id(p_id){
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
     * @brief returns the id of the sensor
     * @return id of the sensor
     */
    uint8_t id(){return _id;}

    private:
    uint8_t _id;
    virtual bool isValid()=0;
    virtual void readRaw(float & buffer)=0;
};
