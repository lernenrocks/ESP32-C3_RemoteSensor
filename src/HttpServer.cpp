#include <WiFi.h>
#include "SensorManager.h"

#define BUFFER_SIZE 512
namespace
{
    WiFiServer server(80);
    char requestHeader[BUFFER_SIZE] = {};

    const char getSensors[] = "GET /sensors";
    const char getStatus[] = "GET /status";//not implemented yet
    const char getCalibrationInfo[] = "GET /calibrationinfo";
    const char postCalibrate[] = "POST /calibrate";//not implemented yet

    void printSensorInfo(WiFiClient &client)
    {
        char buffer[BUFFER_SIZE] = {};
        SensorManager::getSensorDataJson(buffer, sizeof(buffer));
        client.printf("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: %d\r\n\r\n", strlen(buffer));
        client.print(buffer);
    }
    void printCalibrationInfo(WiFiClient &client)
    {
        char buffer[BUFFER_SIZE] = {};
        SensorManager::getCalibrationInfoJson(buffer, sizeof(buffer));
        client.printf("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: %d\r\n\r\n", strlen(buffer));
        client.print(buffer);
    }
}
namespace HttpServer
{

    void begin()
    {
        server.begin();
    }
    void end()
    {
        server.end();
    }
    void handle()
    {
        WiFiClient client = server.available();
        if (!client)
            return;
        memset(requestHeader, 0, BUFFER_SIZE);
        int idx = 0;
        while (client.connected() && idx < BUFFER_SIZE - 1)
        {
            if (client.available())
            {
                requestHeader[idx++] = client.read();
                if (idx >= 4 &&
                    requestHeader[idx - 4] == '\r' && requestHeader[idx - 3] == '\n' &&
                    requestHeader[idx - 2] == '\r' && requestHeader[idx - 1] == '\n')
                {
                    break;
                }
            }
        }
        Serial.println(requestHeader); // debug: output
        if (strstr(requestHeader, getSensors) != 0)
        {
            printSensorInfo(client);
        }
        else if(strstr(requestHeader,getCalibrationInfo)!=0){
            printCalibrationInfo(client);
        }
        else
        {
            client.println("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 2\r\n\r\n{}");
        }
        client.stop();
    }
}