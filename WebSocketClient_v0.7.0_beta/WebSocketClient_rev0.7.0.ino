/**
 * @file    BasicHTTPClient.ino
 * @version 0.7.0 (beta)
 * 
 * Dual licensed under the MIT or GPL Version 2 (2.1) licenses.
 * Copyright (c) 2016 visyeii
 * 
 */

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#include "wsBasicHttpClient.h"
#include "webSocket.h"

#define USE_SERIAL Serial


const char* ssid = "ssid";
const char* password = "password";
ESP8266WiFiMulti WiFiMulti;

void setup() {

  USE_SERIAL.begin(115200);
  // USE_SERIAL.setDebugOutput(true);

  USE_SERIAL.println();
  USE_SERIAL.println();
  USE_SERIAL.println();

  for (uint8_t t = 4; t > 0; t--) {
    USE_SERIAL.printf("[SETUP] WAIT %d...\n", t);
    USE_SERIAL.flush();
    delay(1000);
  }

  WiFiMulti.addAP(ssid, password);

  webSocket_init();
}

wsHTTPClient g_http;

void loop() {
  // wait for WiFi connection
  if (!webSocket_isStart())
  {
    if ((WiFiMulti.run() == WL_CONNECTED))
    {
      USE_SERIAL.print("[HTTP] begin...\n");
      // configure traged server and url
      g_http.begin("http://192.168.24.10:8080/test_chat/server.php"); //HTTP
      g_http.setReuse(true);//keep-alive
      g_http.setUpgrade(true);//keep-Upgrade
      g_http.addHeader("Upgrade", "websocket");
      g_http.addHeader("Sec-WebSocket-Version", "13");
      g_http.addHeader("Sec-WebSocket-Key", "435M9dxxxAaUNpvBi0PRPA==");

      USE_SERIAL.print("[HTTP] GET...\n");
      // start connection and send HTTP header
      int httpCode = g_http.GET();

      // httpCode will be negative on error
      if (httpCode > 0)
      {
        // HTTP header has been send and Server response header has been handled
        USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);

        // file found at server
        if (httpCode == HTTP_CODE_OK)
        {
          String payload = g_http.getString();
          USE_SERIAL.println(payload);
        }
        else if (httpCode == HTTP_CODE_SWITCHING_PROTOCOLS)
        {
          USE_SERIAL.println("HTTP_CODE_SWITCHING_PROTOCOLS");
          wsSetHandles();
          webSocket_start();
        }
      }
      else
      {
        USE_SERIAL.printf("[HTTP] GET... failed, error: %s\n", g_http.errorToString(httpCode).c_str());
      }

      if (!webSocket_isStart())
      {
        g_http.end();
      }
    }

    delay(10000);
  }
  else
  {
    WiFiClient g_client;
    g_client = g_http.getStream();
    webSocket_handle(g_client);

    if (USE_SERIAL.available())
    {
      String str = "";
      str = "{\"message\":\"" + USE_SERIAL.readString() + "\",\"name\":\"ESPr\",\"color\":\"F00\"}";
      webSocket_setData(str);
      USE_SERIAL.println(str);
    }
  }

}

void wsSetHandles(void)
{
  webSocket_setUseMask(true);
  webSocket_setMode(WEBSOCKET_MODE_CLIENT);
  webSocket_setRefreshMask(0x00, 0x00, 0x00, 0x00);
  webSocket_setHandler(WEBSOCKET_HANDLER_OPEN, handleWebSocketOpen);
  webSocket_setHandler(WEBSOCKET_HANDLER_TIMEOUT_RETRY,
                       handleWebSocketRetry); //send ping
  webSocket_setHandler(WEBSOCKET_HANDLER_PING_RECIVE,
                       handleWebSocketRecivePing);
  //  webSocket_setHandler(WEBSOCKET_HANDLER_PONG_RECIVE,
  //      handleWebSocketRecivePong);
  webSocket_setHandler(WEBSOCKET_HANDLER_RECIVE, handleWebSocketRecive);
  webSocket_setHandler(WEBSOCKET_HANDLER_CLOSE, handleWebSocketClose);
  webSocket_setHandler(WEBSOCKET_HANDLER_TIMEOUT_CLOSE,
                       handleWebSocketTimeOut);
}

void handleWebSocketOpen(void)
{
  Serial.println("handleWebSocketOpen>>>>>>>>>>>>>>>>>>>>");
  String str = "{\"message\":\"Hello WebSocket\",\"name\":\"ESPr\",\"color\":\"F00\"}";
  webSocket_setData(str);
  USE_SERIAL.println(str);
}

void handleWebSocketClose(void)
{
  Serial.println("<<<<<<<<<<<<<<<<<<<<handleWebSocketClose");
  webSocket_setTimeOutRetryCount(0);
  Serial.print("Retry: ");
  Serial.print(webSocket_getTimeOutRetryCount());
  Serial.print("/");
  Serial.print(webSocket_getTimeOutRetryMax());
}

void handleWebSocketRecive(void)
{
  char buff[WEB_SOCKET_PAYLOAD_SIZE];
  String str = "";
  int len = 0;

  len = webSocket_available();

  if (len)
  {
    memset(buff, '\0', WEB_SOCKET_PAYLOAD_SIZE);

    Serial.println("----------handleWebSocketRecive----------");

    webSocket_readBytes((byte *) buff, len);
    str = buff;
    Serial.println(str);

    if (!webSocket_isSendBusy())
    {
      //webSocket_setData(str); // echo back
    }

    Serial.println("----------handleWebSocketRecive----------");
  }
}

void handleWebSocketRetry(void)
{
  Serial.println("<<<<<<<<<<handleWebSocketRetry>>>>>>>>>>");
  Serial.print("Retry: ");
  Serial.print(webSocket_getTimeOutRetryCount());
  Serial.print("/");
  Serial.print(webSocket_getTimeOutRetryMax());
  Serial.println(" Send Ping.");
}

//void handleWebSocketRecivePong(void)
//{
//  Serial.println("Recive pong. Client still alive.");
//}

void handleWebSocketRecivePing(void)
{
  Serial.println("Recive ping. Client still alive.");
  webSocket_sendPong();
}

void handleWebSocketTimeOut(void)
{
  Serial.print("TimeOut: ");
  Serial.print(webSocket_getTimeOutRetryCount());
  Serial.print("/");
  Serial.println(webSocket_getTimeOutRetryMax());
}

