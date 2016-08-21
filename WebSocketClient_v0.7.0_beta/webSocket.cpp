/*
 * @file    webSocket.cpp
 * @version 0.7.0 (beta)
 * 
 * Dual licensed under the MIT or GPL Version 2 (2.1) licenses.
 * Copyright (c) 2016 visyeii
 * 
 * webSocket_Hash_Key has been diverted from EasyWebSocket::Hash_Key[at EazyWebSocket.cpp]
 * Copyright 2016 mgo-tec
 * Released under Dual licensed under the MIT or GPL Version 2 (2.1) licenses.
 */
 
#include <HardwareSerial.h>
#include <Print.h>
#include <WiFiClient.h>
#include <cstdbool>
#include <cstdint>
#include "webSocket.h"
#include "Hash.h"

#define WEBSOCKET_DEBUG
#define WEB_SOCKET_HEAD_FRAME_SIZE	2
#define WEB_SOCKET_MASK_KEY_SIZE	4
#define WEB_SOCKET_HEADER_SIZE		(WEB_SOCKET_HEAD_FRAME_SIZE + WEB_SOCKET_MASK_KEY_SIZE)

enum webSocetStateCode
{
  WEBSOCET_STATE_NONE = 0x00,
  WEBSOCET_STATE_RECIVE = 0x01,
  WEBSOCET_STATE_SEND = 0x02,
  WEBSOCET_STATE_HANDSHAKE = 0x03,
  WEBSOCET_STATE_CLOSE = 0x08,
  WEBSOCET_STATE_OPEN = 0x10,
  WEBSOCET_STATE_CLOSING = 0x14,
  WEBSOCET_STATE_CLOSING_RECIVE = 0x15,
  WEBSOCET_STATE_CLOSING_SEND = 0x16,
  WEBSOCET_STATE_CLOSING_END = 0x17
};

enum webSocetFrameOpcode
{
  OPCODE_FRAME_CONTINUE = 0x00,
  OPCODE_FRAME_TEXT = 0x01,
  OPCODE_FRAME_BINARY = 0x02,
  OPCODE_FRAME_RSV1 = 0x03,
  OPCODE_FRAME_RSV2 = 0x04,
  OPCODE_FRAME_RSV3 = 0x05,
  OPCODE_FRAME_RSV4 = 0x06,
  OPCODE_FRAME_RSV5 = 0x07,
  OPCODE_FRAME_CLOSE = 0x08,
  OPCODE_FRAME_PING = 0x09,
  OPCODE_FRAME_PONG = 0x0A,
  OPCODE_FRAME_RSV8 = 0x0B,
  OPCODE_FRAME_RSV9 = 0x0C,
  OPCODE_FRAME_RSV10 = 0x0D,
  OPCODE_FRAME_RSV11 = 0x0E,
  OPCODE_FRAME_RSV12 = 0x0F
};

typedef struct _WEB_SOCKET_FRAME_HEADER_INFO
{
  uint8_t opcode : 4;
  uint8_t rsv3 : 1;
  uint8_t rsv2 : 1;
  uint8_t rsv1 : 1;
  uint8_t fin : 1;
  uint8_t payload_length : 7;
  uint8_t masked : 1;
} WEB_SOCKET_FRAME_HEADER_INFO;

typedef union _WEB_SOCKET_FRAME_HEADER
{
  WEB_SOCKET_FRAME_HEADER_INFO data;
  char byte[2];
} WEB_SOCKET_FRAME_HEADER;

static uint16_t webSocket_getPayloadType(uint16_t payload_length);
static void webSocket_setPayload(const char *payload, uint16_t payload_length, uint8_t payload_option);
static void webSocket_encodeMask(const char *payload, uint16_t payload_length, uint8_t payload_option);
static void webSocket_clear(void);
static void webSocket_timeOutRefresh(void);
static bool webSocket_is_timeOutElapse(void);
static bool webSocket_is_timeOutRetryOver(void);
//static void webSocket_stop(void);
static void webSocket_stateControl(WiFiClient client);
static void webSocket_stateControlOpen(void);
static void webSocket_stateControlClosing(void);
static void webSocket_send(WiFiClient client);
static void webSocket_readFrameHeader(WiFiClient client);
static void webSocket_readFramePayload(WiFiClient client);

static int webSocket_printClientRead(WiFiClient client);
#ifndef WEBSOCKET_DEBUG
static void webSocket_printWriteData(uint8_t payload_length);
static void webSocket_printFrameHeader(void);
static void webSocket_printFramePayload(void);
#endif // WEBSOCKET_DEBUG

static WEB_SOCKET_FRAME_HEADER g_wsHeaderRecive;
static WEB_SOCKET_FRAME_HEADER g_wsHeaderSend;

static char g_webSocketFrameMask[WEB_SOCKET_MASK_KEY_SIZE];
static char g_webSocketReadPayload[WEB_SOCKET_PAYLOAD_SIZE];
static char g_webSocketWriteData[WEB_SOCKET_HEADER_SIZE + WEB_SOCKET_PAYLOAD_SIZE];

static uint8_t g_webSocketMode = WEBSOCKET_MODE_SERVER;
static bool g_is_webSocketStart = false;
static int g_handleLength = 0;
static uint16_t g_sendPayloadLength = 0;
static uint16_t g_recivePayloadLength = 0;
static uint8_t g_webSocketState = 0;
static bool g_is_setSendData = false;
static bool g_is_sendMaskUse = false;
static bool g_is_sendMaskRefresh = false;
static uint32_t g_webSocketTimeoutMax = WEB_SOCKET_TIMEOUT_DEFAULT;//msec
static uint32_t g_webSocketTimeoutCount = 0;//msec
static uint8_t g_webSocketRetryMax = WEB_SOCKET_TIMEOUT_RETRY;//msec
static uint8_t g_webSocketRetryCount = 0;//msec
static webSocketHandler g_webSocketHandleOpen = NULL;
static webSocketHandler g_webSocketHandleSend = NULL;
static webSocketHandler g_webSocketHandleReceive = NULL;
static webSocketHandler g_webSocketHandleTimeOutRetry = NULL;
static webSocketHandler g_webSocketHandleTimeOutClose = NULL;
//static webSocketHandler g_webSocketPingSend = NULL; //g_webSocketTimeOutRetry
static webSocketHandler g_webSocketHandleReceivePing = NULL;
static webSocketHandler g_webSocketHandleReceivePong = NULL;
static webSocketHandler g_webSocketHandleClose = NULL;
static webSocketHandler g_webSocketHandleRefreshMask = NULL;

void webSocket_init(void)
{
  webSocket_clear();
}

void webSocket_handlerWrapper(webSocketHandler handler)
{
  if (handler != NULL)
  {
    handler();
  }
}

void webSocket_setHandler(webSocketHandlerType type, webSocketHandler handler)
{
  switch (type)
  {
    case WEBSOCKET_HANDLER_OPEN:
      g_webSocketHandleOpen = handler;
      break;
    case WEBSOCKET_HANDLER_SEND:
      g_webSocketHandleSend = handler;
      break;
    case WEBSOCKET_HANDLER_RECIVE:
      g_webSocketHandleReceive = handler;
      break;
    case WEBSOCKET_HANDLER_TIMEOUT_RETRY://send ping
      g_webSocketHandleTimeOutRetry = handler;
      break;
    case WEBSOCKET_HANDLER_TIMEOUT_CLOSE:
      g_webSocketHandleTimeOutClose = handler;
      break;
    case WEBSOCKET_HANDLER_PING_RECIVE:
      g_webSocketHandleReceivePing = handler;
      break;
    case WEBSOCKET_HANDLER_PONG_RECIVE:
      g_webSocketHandleReceivePong = handler;
      break;
    case WEBSOCKET_HANDLER_CLOSE:
      g_webSocketHandleClose = handler;
      break;
    case WEBSOCKET_HANDLER_MASK_REFRESH:
      g_webSocketHandleRefreshMask = handler;
      break;
  }
}

void webSocket_start(void)
{
  if ((g_webSocketState == WEBSOCET_STATE_CLOSE)
      || (g_webSocketState == WEBSOCET_STATE_NONE))
  {
    g_webSocketState = WEBSOCET_STATE_OPEN;
    g_is_webSocketStart = true;
  }
#ifndef WEBSOCKET_DEBUG
  Serial.print("webSocket_start(): "); // DEBUG
  Serial.print("g_webSocketState: "); // DEBUG
  Serial.print(g_webSocketState); // DEBUG
  Serial.print(" g_is_webSocketStart: "); // DEBUG
  Serial.println(g_is_webSocketStart); // DEBUG
#endif // WEBSOCKET_DEBUG

  webSocket_timeOutRefresh();
  g_webSocketRetryCount = 0;
  webSocket_handlerWrapper(g_webSocketHandleOpen);
}

void webSocket_setMode(uint8_t mode)
{
  if (mode == WEBSOCKET_MODE_SERVER)
  {
    g_webSocketMode = WEBSOCKET_MODE_SERVER;
  }
  else
  {
    g_webSocketMode = WEBSOCKET_MODE_CLIENT;
  }
}

void webSocket_setTimeoutMax(uint32_t max)
{
  if (WEB_SOCKET_TIMEOUT_MIN < max)
  {
    g_webSocketTimeoutMax = WEB_SOCKET_TIMEOUT_MIN;
  }
  else
  {
    g_webSocketTimeoutMax = max;
  }
}

void webSocket_setTimeOutRetryMax(uint8_t max)
{
  g_webSocketRetryMax = max;
}

void webSocket_setTimeOutRetryCount(uint8_t count)
{
  g_webSocketRetryCount = count;
}

uint8_t webSocket_getTimeOutRetryMax(void)
{
  return g_webSocketRetryMax;
}

uint8_t webSocket_getTimeOutRetryCount(void)
{
  return g_webSocketRetryCount;
}

bool webSocket_isStart(void)
{
  return g_is_webSocketStart;
}

void webSocket_handle(WiFiClient client)
{
  g_handleLength = client.available();

  if (g_handleLength >= 2)
  {
    //		while(client.available()){
    //			webSocket_printClientRead(client);
    //		}

    // received
    webSocket_readFrameHeader(client);
#ifndef WEBSOCKET_DEBUG
    webSocket_printFrameHeader(); // DEBUG
#endif // WEBSOCKET_DEBUG
    webSocket_readFramePayload(client);
#ifndef WEBSOCKET_DEBUG
    webSocket_printFramePayload(); // DEBUG
#endif // WEBSOCKET_DEBUG
    webSocket_timeOutRefresh();
    g_webSocketRetryCount = 0;

    webSocket_handlerWrapper(g_webSocketHandleReceive);
  }

  if (g_webSocketMode == WEBSOCKET_MODE_SERVER)
  {
    if ((g_webSocketState == WEBSOCET_STATE_OPEN)
        || (g_webSocketState == WEBSOCET_STATE_CLOSING))
    {
      if (webSocket_is_timeOutElapse())
      {
        if (webSocket_is_timeOutRetryOver())
        {
          g_webSocketState = WEBSOCET_STATE_CLOSE;
          webSocket_sendClose();
          webSocket_handlerWrapper(g_webSocketHandleTimeOutClose);
#ifndef WEBSOCKET_DEBUG
          Serial.println("TIMEOUT: CLOSE"); // DEBUG
#endif // WEBSOCKET_DEBUG
        }
        else
        {
          g_webSocketRetryCount++;
          webSocket_timeOutRefresh();
          webSocket_sendPing();
          webSocket_handlerWrapper(g_webSocketHandleTimeOutRetry);
#ifndef WEBSOCKET_DEBUG
          Serial.print("TIMEOUT: RETRY"); // DEBUG
          Serial.print(g_webSocketRetryCount);// DEBUG
          Serial.print("/");// DEBUG
          Serial.println(g_webSocketRetryMax);// DEBUG
#endif // WEBSOCKET_DEBUG
        }
      }
    }
  }

  webSocket_stateControl(client);
}

void webSocket_setData(String sendString)
{
  if (sendString.length() <= WEB_SOCKET_PAYLOAD_SIZE)
  {
    webSocket_setData(sendString.c_str(), sendString.length(),
                      OPCODE_FRAME_TEXT);
  }
  else
  {
#ifndef WEBSOCKET_DEBUG
    Serial.println("setData(): length too long"); // DEBUG
#endif // WEBSOCKET_DEBUG
  }
}

void webSocket_sendPong(void)
{
  webSocket_setData(NULL, 0, OPCODE_FRAME_PONG);
}

void webSocket_sendPing(void)
{
  webSocket_setData(NULL, 0, OPCODE_FRAME_PING);
}

void webSocket_sendClose(void)
{
  webSocket_setData(NULL, 0, OPCODE_FRAME_CLOSE);
  g_webSocketState |= WEBSOCET_STATE_SEND;
}

void webSocket_setUseMask(bool flag)
{
  g_is_sendMaskUse = flag;
}

void webSocket_setRefreshMask(byte mask1, byte mask2, byte mask3, byte mask4)
{
  g_is_sendMaskRefresh = true;
  g_webSocketFrameMask[0] = mask1;
  g_webSocketFrameMask[1] = mask2;
  g_webSocketFrameMask[2] = mask3;
  g_webSocketFrameMask[3] = mask4;
}

bool webSocket_isSendBusy(void)
{
  return g_is_setSendData;
}

void webSocket_setData(const char *payload, uint16_t payload_length,
                       uint8_t opcode)
{
  uint8_t payload_option = 0;

  if (!webSocket_isSendBusy())
  {
    g_wsHeaderSend.data.fin = 1;
    g_wsHeaderSend.data.opcode = opcode;
    g_wsHeaderSend.data.masked = g_is_sendMaskUse;

    payload_option = webSocket_getPayloadType(payload_length);
    g_sendPayloadLength = payload_length;

    if (payload_option)
    {
      g_wsHeaderSend.data.payload_length = WEB_SOCKET_PAYLOAD_TYPE2_FLAG;
    }
    else
    {
      g_wsHeaderSend.data.payload_length = payload_length;
    }

    memcpy(&g_webSocketWriteData, g_wsHeaderSend.byte,
           WEB_SOCKET_HEAD_FRAME_SIZE);

    if (g_is_sendMaskUse)
    {
      memcpy(&g_webSocketWriteData[WEB_SOCKET_HEAD_FRAME_SIZE + payload_option],
             g_webSocketFrameMask, WEB_SOCKET_MASK_KEY_SIZE);
    }

    if (payload_option)
    {
      g_webSocketWriteData[WEB_SOCKET_HEAD_FRAME_SIZE] = (char)(payload_length >> 8);
      g_webSocketWriteData[WEB_SOCKET_HEAD_FRAME_SIZE + 1] = (char)(payload_length & 0x00FF);
    }

    webSocket_setPayload(payload, payload_length, payload_option);

#ifndef WEBSOCKET_DEBUG
    webSocket_printWriteData(payload_length); // DEBUG
#endif // WEBSOCKET_DEBUG

    g_is_setSendData = true;
  }

}

static uint16_t webSocket_getPayloadType(uint16_t payload_length)
{
  uint16_t size = 0;

  if (payload_length <= WEB_SOCKET_PAYLOAD_TYPE1)
  {
    size = 0;
  }
  else if (payload_length <= WEB_SOCKET_PAYLOAD_TYPE2)
  {
    size = 2;
  }

  return size;
}

static void webSocket_setPayload(const char *payload, uint16_t payload_length, uint8_t payload_option)
{
  if (payload_length && payload != NULL)
  {
    uint8_t mask_index = 0;

    if (g_is_sendMaskUse)
    {
      webSocket_encodeMask(payload, payload_length, payload_option);

      g_is_sendMaskRefresh = false;
    }
    else
    {
      memcpy(&g_webSocketWriteData[WEB_SOCKET_HEAD_FRAME_SIZE + payload_option],
             payload, payload_length);
    }
  }
}

static void webSocket_encodeMask(const char *payload, uint16_t payload_length, uint8_t payload_option)
{
  uint16_t mask_index = 0;

  for (uint16_t i = 0; i < payload_length; i++)
  {
    g_webSocketWriteData[WEB_SOCKET_HEADER_SIZE + payload_option + i] =
      payload[i] ^ g_webSocketFrameMask[mask_index];

    mask_index++;

    if (mask_index >= WEB_SOCKET_MASK_KEY_SIZE)
    {
      mask_index = 0;
    }
  }
}

int webSocket_available(void)
{
  return g_recivePayloadLength;
}

void webSocket_readBytes(byte *dist, uint16_t payload_length)
{
  memcpy(dist, g_webSocketReadPayload, payload_length);
  g_wsHeaderRecive.data.payload_length = 0;
  g_recivePayloadLength = 0;
}

static void webSocket_clear(void)
{
  g_webSocketHandleOpen = NULL;
  g_webSocketHandleSend = NULL;
  g_webSocketHandleReceive = NULL;
  g_webSocketHandleTimeOutRetry = NULL;
  g_webSocketHandleTimeOutClose = NULL;
  g_webSocketHandleReceivePong = NULL;
  g_webSocketHandleClose = NULL;

  g_webSocketMode = WEBSOCKET_MODE_SERVER;
  g_webSocketState = WEBSOCET_STATE_NONE;
  g_is_webSocketStart = false;
  g_is_setSendData = false;
  g_is_sendMaskUse = false;
  g_is_sendMaskRefresh = false;

  g_handleLength = 0;
  g_sendPayloadLength = 0;
  g_recivePayloadLength = 0;

  g_wsHeaderRecive.byte[0] = 0x00;
  g_wsHeaderRecive.byte[1] = 0x00;

  g_wsHeaderSend.byte[0] = 0x00;
  g_wsHeaderSend.byte[1] = 0x00;

  g_webSocketFrameMask[0] = 0;
  g_webSocketFrameMask[1] = 0;
  g_webSocketFrameMask[2] = 0;
  g_webSocketFrameMask[3] = 0;

  g_webSocketTimeoutMax = WEB_SOCKET_TIMEOUT_DEFAULT;//msec
  g_webSocketRetryMax = WEB_SOCKET_TIMEOUT_RETRY;
  g_webSocketRetryCount = 0;
}

static void webSocket_timeOutRefresh(void)
{
  g_webSocketTimeoutCount = millis();
}

static bool webSocket_is_timeOutElapse(void)
{
  if (millis() - g_webSocketTimeoutCount >= g_webSocketTimeoutMax)
  {
    return true;
  }
  else
  {
    return false;
  }
}

static bool webSocket_is_timeOutRetryOver(void)
{
  if (g_webSocketRetryCount >= g_webSocketRetryMax)
  {
    return true;
  }
  else
  {
    return false;
  }
}

//static void webSocket_stop(void)
//{
//	if (!(g_webSocketState & WEBSOCET_STATE_CLOSE))
//	{
//		g_webSocketState |= WEBSOCET_STATE_CLOSING;
//
//		if (!(g_webSocketState & WEBSOCET_STATE_SEND))
//		{
//			webSocket_close();
//		}
//	}
//	Serial.print("webSocket_stop(): "); // DEBUG
//	Serial.print("g_webSocketState: "); // DEBUG
//	Serial.print(g_webSocketState); // DEBUG
//	Serial.print("g_is_webSocketStart: "); // DEBUG
//	Serial.println(g_is_webSocketStart); // DEBUG
//}

static void webSocket_stateControl(WiFiClient client)
{
  switch (g_webSocketState & ~(WEBSOCET_STATE_HANDSHAKE))
  {
    case WEBSOCET_STATE_NONE:
      break;
    case WEBSOCET_STATE_OPEN:
      webSocket_stateControlOpen();
      break;
    case WEBSOCET_STATE_CLOSING:
      webSocket_stateControlClosing();
      break;
    case WEBSOCET_STATE_CLOSE:
      webSocket_handlerWrapper(g_webSocketHandleClose);
      webSocket_clear();
      client = WiFiClient(); // dissconnect
      break;
    default:
      break;
  }

  if (g_is_sendMaskRefresh == false && g_is_setSendData == false)
  {
    webSocket_handlerWrapper(g_webSocketHandleRefreshMask);
  }

  webSocket_send(client);

  g_wsHeaderRecive.byte[0] = 0;
  g_wsHeaderRecive.byte[1] = 0;
}

static void webSocket_stateControlOpen(void)
{
  switch (g_wsHeaderRecive.data.opcode)
  {
    case OPCODE_FRAME_CLOSE:
      g_webSocketState = WEBSOCET_STATE_CLOSING;
      g_webSocketState |= WEBSOCET_STATE_RECIVE;
      webSocket_setData(NULL, 0, OPCODE_FRAME_CLOSE);
#ifndef WEBSOCKET_DEBUG
      Serial.println("OPEN: RECIVE OPCODE_FRAME_CLOSE"); // DEBUG
#endif // WEBSOCKET_DEBUG
      break;
    case OPCODE_FRAME_PING:
      // not supported
#ifndef WEBSOCKET_DEBUG
      Serial.println("OPEN: RECIVE OPCODE_FRAME_PING"); // DEBUG
#endif // WEBSOCKET_DEBUG
      webSocket_handlerWrapper(g_webSocketHandleReceivePing);
      break;
    case OPCODE_FRAME_PONG:
#ifndef WEBSOCKET_DEBUG
      Serial.println("OPEN: RECIVE OPCODE_FRAME_PONG"); // DEBUG
#endif // WEBSOCKET_DEBUG
      webSocket_handlerWrapper(g_webSocketHandleReceivePong);
      break;
    default:
      if (g_recivePayloadLength)
      {
#ifndef WEBSOCKET_DEBUG
        Serial.println("OPEN: test echo"); // DEBUG
        Serial.print("g_is_setSendData: "); // DEBUG
        Serial.println(g_is_setSendData); // DEBUG
#endif // WEBSOCKET_DEBUG
      }
      break;
  }
}

static void webSocket_stateControlClosing(void)
{
  switch (g_wsHeaderRecive.data.opcode)
  {
    case OPCODE_FRAME_CLOSE:
      g_webSocketState |= WEBSOCET_STATE_RECIVE;
#ifndef WEBSOCKET_DEBUG
      Serial.println("CLOSING: RECIVE OPCODE_FRAME_CLOSE"); // DEBUG
#endif // WEBSOCKET_DEBUG
      break;
    case OPCODE_FRAME_PING:
      // not supported
#ifndef WEBSOCKET_DEBUG
      Serial.println("CLOSING: PING"); // DEBUG
#endif // WEBSOCKET_DEBUG
      break;
    case OPCODE_FRAME_PONG:
      // not supported
#ifndef WEBSOCKET_DEBUG
      Serial.println("CLOSING: PONG"); // DEBUG
#endif // WEBSOCKET_DEBUG
      break;
    default:
      break;
  }

  if (!(g_webSocketState & WEBSOCET_STATE_SEND))
  {
    webSocket_sendClose();
#ifndef WEBSOCKET_DEBUG
    Serial.println("CLOSING: SEND OPCODE_FRAME_CLOSE"); // DEBUG
#endif // WEBSOCKET_DEBUG
  }

  if ((g_webSocketState & WEBSOCET_STATE_HANDSHAKE)
      == WEBSOCET_STATE_HANDSHAKE)
  {
    g_webSocketState = WEBSOCET_STATE_CLOSE;
#ifndef WEBSOCKET_DEBUG
    Serial.println("CLOSING: CLOSE"); // DEBUG
#endif // WEBSOCKET_DEBUG
  }
}

static void webSocket_send(WiFiClient client)
{
  uint16 payload_option = 0;

  payload_option = webSocket_getPayloadType(g_sendPayloadLength);

  if (client && g_is_webSocketStart && g_is_setSendData)
  {
    if (g_is_sendMaskUse)
    {
      client.write((const char *) g_webSocketWriteData,
                   WEB_SOCKET_HEADER_SIZE + payload_option
                   + g_sendPayloadLength);
    }
    else
    {
      client.write((const char *) g_webSocketWriteData,
                   WEB_SOCKET_HEAD_FRAME_SIZE + payload_option
                   + g_sendPayloadLength);
    }
    g_is_setSendData = false;

    g_wsHeaderSend.byte[0] = 0;
    g_wsHeaderSend.byte[1] = 0;
    g_sendPayloadLength = 0;
    webSocket_handlerWrapper(g_webSocketHandleSend);
  }
}

static void webSocket_readFrameHeader(WiFiClient client)
{
  g_wsHeaderRecive.byte[0] = webSocket_printClientRead(client);
  g_wsHeaderRecive.byte[1] = webSocket_printClientRead(client);

  g_recivePayloadLength = g_wsHeaderRecive.data.payload_length;

  if (g_wsHeaderRecive.data.payload_length == WEB_SOCKET_PAYLOAD_TYPE2_FLAG)
  {
    char bit16_high = 0;
    char bit16_low = 0;

    bit16_high  = webSocket_printClientRead(client);
    bit16_low  = webSocket_printClientRead(client);

    g_recivePayloadLength = (((uint16_t)bit16_high) << 8) | ((uint16_t)bit16_low);
  }

  if (g_wsHeaderRecive.data.masked)
  {
    if (g_handleLength >= 6)
    {
      g_webSocketFrameMask[0] = webSocket_printClientRead(client);
      g_webSocketFrameMask[1] = webSocket_printClientRead(client);
      g_webSocketFrameMask[2] = webSocket_printClientRead(client);
      g_webSocketFrameMask[3] = webSocket_printClientRead(client);
    }
    else
    {
      // error
    }
  }
}

static void webSocket_readFramePayload(WiFiClient client)
{
  int c = 0;
  char b = 0;
  int mask_index = 0;
  int payload_count = 0;

  while (g_recivePayloadLength > payload_count)
  {
    if (client.available() == 0)
    {
#ifndef WEBSOCKET_DEBUG
      Serial.println("PAYLOAD_SIZE MISS MATCH:"); // DEBUG
#endif // WEBSOCKET_DEBUG
      break;
    }

    c = webSocket_printClientRead(client);
    b = (char) c;
    g_webSocketReadPayload[payload_count] = (b
                                            ^ g_webSocketFrameMask[mask_index]);

    mask_index++;

    if (mask_index >= 4)
    {
      mask_index = 0;
    }

    payload_count++;

    if (payload_count >= WEB_SOCKET_PAYLOAD_SIZE)
    {
#ifndef WEBSOCKET_DEBUG
      Serial.print("PAYLOAD_SIZE OVER:"); // DEBUG
      Serial.println(payload_count);
#endif // WEBSOCKET_DEBUG
      break;		// not supported
    }
  }
}
#ifndef WEBSOCKET_DEBUG
static void webSocket_printWriteData(uint8_t payload_length)
{
  Serial.println();
  int len = 0;

  if (g_is_sendMaskUse)
  {
    len = WEB_SOCKET_HEADER_SIZE + payload_length;
  }
  else
  {
    len = WEB_SOCKET_HEAD_FRAME_SIZE + payload_length;
  }

  for (int i = 0; i < len; i++)
  {
    Serial.print(g_webSocketWriteData[i], HEX);
    Serial.print(", ");
  }
  Serial.println();
}

#endif // WEBSOCKET_DEBUG
// It has been diverted from EazyWebSocket.cpp copyright 2016 mgo-tec
void webSocket_Hash_Key(String h_req_key, char* h_resp_key)
{
  const char* GUID_str = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  char Base64[65] =
  { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b',
    'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1',
    '2', '3', '4', '5', '6', '7', '8', '9', '+', '/', '='
  };
  byte hash_six[28];//27array subscript is above array bounds
  byte dummy_h1, dummy_h2;
  byte i, j;
  i = 0;
  j = 0;

  String merge_str;

  merge_str = h_req_key + String(GUID_str);
#ifndef WEBSOCKET_DEBUG
  Serial.println(F("--------------------Hash key Generation"));
  Serial.print(F("merge_str ="));
  Serial.println(merge_str);
  Serial.print(F("SHA1:"));
  Serial.println(sha1(merge_str));
#endif // WEBSOCKET_DEBUG

  byte hash[20];
  sha1(merge_str, &hash[0]);

  for (i = 0; i < 20; i++)
  {
    hash_six[j] = hash[i] >> 2;

    hash_six[j + 1] = hash[i + 1] >> 4;
    bitWrite(hash_six[j + 1], 4, bitRead(hash[i], 0));
    bitWrite(hash_six[j + 1], 5, bitRead(hash[i], 1));

    if (j + 2 < 26)
    {
      hash_six[j + 2] = hash[i + 2] >> 6;
      bitWrite(hash_six[j + 2], 2, bitRead(hash[i + 1], 0));
      bitWrite(hash_six[j + 2], 3, bitRead(hash[i + 1], 1));
      bitWrite(hash_six[j + 2], 4, bitRead(hash[i + 1], 2));
      bitWrite(hash_six[j + 2], 5, bitRead(hash[i + 1], 3));
    }
    else if (j + 2 == 26)
    {
      dummy_h1 = 0;
      dummy_h2 = 0;
      dummy_h2 = hash[i + 1] << 4;
      dummy_h2 = dummy_h2 >> 2;
      hash_six[j + 2] = dummy_h1 | dummy_h2;
    }

    if (j + 3 < 27)
    {
      hash_six[j + 3] = hash[i + 2];
      bitWrite(hash_six[j + 3], 6, 0);
      bitWrite(hash_six[j + 3], 7, 0);
    }
    else if (j + 3 == 27)
    {
      hash_six[(j + 3)] = '=';//array subscript is above array bounds
    }

    h_resp_key[j] = Base64[hash_six[j]];
    h_resp_key[j + 1] = Base64[hash_six[j + 1]];
    h_resp_key[j + 2] = Base64[hash_six[j + 2]];

    if (j + 3 == 27)
    {
      h_resp_key[j + 3] = Base64[64];
      break;
    }
    else
    {
      h_resp_key[j + 3] = Base64[hash_six[j + 3]];
    }

    i = i + 2;
    j = j + 4;
  }
  h_resp_key[28] = '\0';

#ifndef WEBSOCKET_DEBUG
  Serial.print(F("hash_six = "));
  for (i = 0; i < 28; i++)
  {
    Serial.print(hash_six[i], BIN);
    Serial.print('_');
  }
  Serial.println();
#endif // WEBSOCKET_DEBUG
}


static int webSocket_printClientRead(WiFiClient client)
{
  int read_char = 0;

  read_char = client.read();

#ifndef WEBSOCKET_DEBUG
  Serial.print("0x");
  Serial.print(read_char, HEX);
  Serial.print("=[");
  Serial.print((char)read_char);
  Serial.print("], ");
#endif // WEBSOCKET_DEBUG

  return read_char;
}
#ifndef WEBSOCKET_DEBUG
static void webSocket_printFrameHeader(void)
{
  Serial.print("FIN: ");
  Serial.print(g_wsHeaderRecive.data.fin, HEX);
  Serial.println(" ");

  Serial.print("OPCODE: ");
  Serial.print(g_wsHeaderRecive.data.opcode, HEX);
  Serial.print(" ");

  switch (g_wsHeaderRecive.data.opcode)
  {
    case OPCODE_FRAME_CONTINUE:
      Serial.print("OPCODE_FRAME_CONTINUE");
      break;
    case OPCODE_FRAME_BINARY:
      Serial.print("OPCODE_FRAME_BINARY");
      break;
    case OPCODE_FRAME_TEXT:
      Serial.print("OPCODE_FRAME_TEXT");
      break;
    case OPCODE_FRAME_CLOSE:
      Serial.print("OPCODE_FRAME_CLOSE");
      break;
    case OPCODE_FRAME_PING:
      Serial.print("OPCODE_FRAME_PING");
      break;
    case OPCODE_FRAME_PONG:
      Serial.print("OPCODE_FRAME_PONG");
      break;
    default:
      Serial.print("OPCODE_RESERVED");
      break;
  }
  Serial.println();

  if (g_wsHeaderRecive.data.payload_length <= WEB_SOCKET_PAYLOAD_TYPE1)
  {
    Serial.print("PAYLOAD_LENGTH: ");
    Serial.println(g_wsHeaderRecive.data.payload_length, DEC);
  }
  else if  (g_wsHeaderRecive.data.payload_length == WEB_SOCKET_PAYLOAD_TYPE2_FLAG)
  {
    Serial.print("PAYLOAD_LENGTH: ");
    Serial.print(g_wsHeaderRecive.data.payload_length, DEC);
    Serial.print(": ");
    Serial.println(g_recivePayloadLength, DEC);
  }

  Serial.print("MASK: ");
  Serial.println(g_wsHeaderRecive.data.masked, DEC);

  Serial.print("len ");
  Serial.println(g_handleLength, DEC);

  Serial.print("0x");
  Serial.print(g_wsHeaderRecive.byte[0], HEX);
  Serial.print(", 0x");
  Serial.print(g_wsHeaderRecive.byte[1], HEX);
  Serial.println();

  if (g_wsHeaderRecive.data.masked)
  {
    Serial.print("MASK_DATA: ");
    Serial.print("0x");
    Serial.print(g_webSocketFrameMask[0], HEX);
    Serial.print(", 0x");
    Serial.print(g_webSocketFrameMask[1], HEX);
    Serial.print(", 0x");
    Serial.print(g_webSocketFrameMask[2], HEX);
    Serial.print(", 0x");
    Serial.print(g_webSocketFrameMask[3], HEX);
    Serial.println();
  }
}

static void webSocket_printFramePayload(void)
{
  Serial.print("PAYLOAD: ");
  Serial.println(g_wsHeaderRecive.data.payload_length, DEC);

  for (int i = 0; i < g_recivePayloadLength; i++)
  {
    Serial.print("0x");
    Serial.print(g_webSocketReadPayload[i], HEX);
    Serial.print("=[");
    Serial.print(g_webSocketReadPayload[i]);
    Serial.print("], ");
  }
  Serial.println();
}

#endif // WEBSOCKET_DEBUG
