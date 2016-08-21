/*
 * @file    wsBasicHttpClient.cpp
 * @version 0.7.0 (beta)
 * 
 * Dual licensed under the MIT or GPL Version 2 (2.1) licenses.
 * Copyright (c) 2016 visyeii
 * 
 */

#include "wsBasicHttpClient.h"
/**
 * constructor
 */
wsHTTPClient::wsHTTPClient()
{
	HTTPClient();
}

/**
 * destructor
 */
wsHTTPClient::~wsHTTPClient()
{
    if(_tcp) {
        _tcp->stop();
    }
    if(_currentHeaders) {
        delete[] _currentHeaders;
    }
}
void wsHTTPClient::setUpgrade(bool upgrade)
{
    _upgrade = upgrade;
}

int wsHTTPClient::GET()
{
    return sendRequest("GET");
}

int wsHTTPClient::sendRequest(const char * type, uint8_t * payload, size_t size)
{
    // connect to server
    if(!connect()) {
        return returnError(HTTPC_ERROR_CONNECTION_REFUSED);
    }

    if(payload && size > 0) {
        addHeader(F("Content-Length"), String(size));
    }

    // send Header
    if(!sendHeader(type)) {
        return returnError(HTTPC_ERROR_SEND_HEADER_FAILED);
    }

    // send Payload if needed
    if(payload && size > 0) {
        if(_tcp->write(&payload[0], size) != size) {
            return returnError(HTTPC_ERROR_SEND_PAYLOAD_FAILED);
        }
    }

    // handle Server Response (Header)
    return returnError(handleHeaderResponse());
}

bool wsHTTPClient::sendHeader(const char * type)
{
    if(!connected()) {
        return false;
    }

    String header = String(type) + " " + _uri + F(" HTTP/1.");

    if(_useHTTP10) {
        header += "0";
    } else {
        header += "1";
    }

    header += String(F("\r\nHost: ")) + _host +
              F("\r\nUser-Agent: ") + _userAgent +
              F("\r\nConnection: ");

    if(_reuse) {
        header += F("keep-alive");
        if (_upgrade)
        {
            header += F(",Upgrade");
        }
    } else {
        header += F("close");
    }
    header += "\r\n";

    if(!_useHTTP10) {
        header += F("Accept-Encoding: identity;q=1,chunked;q=0.1,*;q=0\r\n");
    }

    if(_base64Authorization.length()) {
        header += F("Authorization: Basic ");
        header += _base64Authorization;
        header += "\r\n";
    }

    header += _headers + "\r\n";

    Serial.print(header);//debug

    return (_tcp->write((const uint8_t *) header.c_str(), header.length()) == header.length());
}
