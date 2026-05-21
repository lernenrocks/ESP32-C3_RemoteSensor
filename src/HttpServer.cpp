#include <WiFi.h>
#define REQUEST_MAX_SIZE 512
namespace
{
    WiFiServer server(80);
    char requestHeader[REQUEST_MAX_SIZE] = {};
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
        int idx = 0;
        while (client.connected() && idx < REQUEST_MAX_SIZE)
        {
            if (client.available())
            {
                requestHeader[idx] = client.read();
                Serial.print(requestHeader[idx]); //! for bugfixing
                idx++;
            }
        }
        client.println("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}");
        client.stop();
    }
}