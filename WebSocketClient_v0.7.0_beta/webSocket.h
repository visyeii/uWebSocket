/*
 * @file    webSocket.h
 * @version 0.7.0 (beta)
 * 
 * Dual licensed under the MIT or GPL Version 2 (2.1) licenses.
 * Copyright (c) 2016 visyeii
 * 
 * webSocket_Hash_Key has been diverted from EasyWebSocket::Hash_Key[at EazyWebSocket.cpp]
 * Copyright 2016 mgo-tec
 * Released under Dual licensed under the MIT or GPL Version 2 (2.1) licenses.
 */
 
#ifndef WEBSOCKET_H_
#define WEBSOCKET_H_

#include "WiFiClient.h"

#define WEB_SOCKET_PAYLOAD_TYPE1		125u

#define WEB_SOCKET_PAYLOAD_TYPE2_FLAG	126u
#define WEB_SOCKET_PAYLOAD_TYPE2		65535u

#define WEB_SOCKET_PAYLOAD_SIZE			(WIFICLIENT_MAX_PACKET_SIZE / 2)
#define WEB_SOCKET_TIMEOUT_RETRY		3u
#define WEB_SOCKET_TIMEOUT_MIN			1000u//msec
#define WEB_SOCKET_TIMEOUT_DEFAULT		2000u//msec

enum webSocketMode {
  WEBSOCKET_MODE_SERVER = 0,
  WEBSOCKET_MODE_CLIENT
};

enum webSocketHandlerType {
  WEBSOCKET_HANDLER_OPEN  = 1,
  WEBSOCKET_HANDLER_SEND,
  WEBSOCKET_HANDLER_RECIVE,
  WEBSOCKET_HANDLER_TIMEOUT_RETRY,
  WEBSOCKET_HANDLER_TIMEOUT_CLOSE,
  WEBSOCKET_HANDLER_PING_RECIVE,
  WEBSOCKET_HANDLER_PONG_RECIVE,
  WEBSOCKET_HANDLER_CLOSE,
  WEBSOCKET_HANDLER_MASK_REFRESH
};

typedef void (*webSocketHandler)(void);

extern void webSocket_setHandler(webSocketHandlerType type,
                                 webSocketHandler handler);
extern void webSocket_init(void);
extern void webSocket_start(void);
extern void webSocket_setMode(uint8_t mode);
extern void webSocket_setTimeoutMax(uint32_t max);
extern void webSocket_setTimeOutRetryMax(uint8_t max);
extern void webSocket_setTimeOutRetryCount(uint8_t count);
extern uint8_t webSocket_getTimeOutRetryMax(void);
extern uint8_t webSocket_getTimeOutRetryCount(void);
extern bool webSocket_isStart(void);
extern void webSocket_handle(WiFiClient client);
extern void webSocket_sendPong(void);
extern void webSocket_sendPing(void);
extern void webSocket_sendClose(void);
extern void webSocket_setData(String sendString);
extern void webSocket_setData(const char *payload, uint16_t payload_length,
                              uint8_t opcode);
extern void webSocket_setUseMask(bool flag);
extern void webSocket_setRefreshMask(byte mask1, byte mask2, byte mask3,
                                     byte mask4);
extern bool webSocket_isSendBusy(void);
extern int webSocket_available(void);
extern void webSocket_readBytes(byte *dist, uint16_t payload_length);
extern void webSocket_Hash_Key(String h_req_key, char* h_resp_key);

#endif /* WEBSOCKET_H_ */
