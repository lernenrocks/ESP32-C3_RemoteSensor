#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
/**
 * @brief Abstract base class for all sensors
 */
class SensorBase
{
public:
SensorBase() = default;
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
     * @brief getter for the currently active calibration values (read-back)
     * @param buffer buffer for the values, JSON object (e.g. {"offset":87244,"scale":0.0234})
     * @param len length of the buffer
     * @return buffer is valid. Sensors without calibration return "{}", still true
     */
    virtual bool getCalibrationValuesJson(char *buffer, size_t len)=0;

    /**
     * @brief get specific precision of the sensor value
     * @return precision after comma
     */
    virtual int getPrecision()=0;

    /**
     * @brief calibrate the sensor with given data
     * @param data provided data for calibration
     * @return true, if calibration is succeeded. Also true if a sensor doesn't need calibration
     */
    virtual bool calibrate(const JsonObjectConst data)=0;

    /**
     * @brief reset calibration to default state (scale=1, offset=0)
     * @return true on success. Sensors without calibration always return true
     */
    virtual void reset()=0;

    private:
    virtual bool isValid()=0;
    virtual void readRaw(float & buffer)=0;
};
