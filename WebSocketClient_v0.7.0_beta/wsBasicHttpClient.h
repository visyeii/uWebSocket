/*
 * @file    wsBasicHttpClient.cpp
 * @version 0.7.0 (beta)
 * 
 * Dual licensed under the MIT or GPL Version 2 (2.1) licenses.
 * Copyright (c) 2016 visyeii
 * 
 */

#ifndef WSBASICHTTPCLIENT_H_
#define WSBASICHTTPCLIENT_H_

#include <ESP8266HTTPClient.h>

class wsHTTPClient: public HTTPClient {

public:
	wsHTTPClient();
    ~wsHTTPClient();

    void setUpgrade(bool upgrade);///upgrade
    int GET();
    int sendRequest(const char * type, uint8_t * payload = NULL, size_t size = 0);

protected:
    bool sendHeader(const char * type);

    bool _upgrade = false;
};

#endif /* WSBASICHTTPCLIENT_H_ */
