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
        Serial.println("server ready");
    }
    void end()
    {
        server.end();
    }
    void handle()
    {
        WiFiClient client = server.available();
        if (!client) return;
        memset(requestHeader, 0, REQUEST_MAX_SIZE);
        int idx = 0;
        while (client.connected() && idx < REQUEST_MAX_SIZE - 1)
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
        Serial.println(requestHeader);
        client.println("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 2\r\n\r\n{}");
        client.stop();
    }
}