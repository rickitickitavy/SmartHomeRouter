#pragma once

#include "Arduino.h"
#include "WiFiClient.h"

#define HTTP_CODE_OK 200
#define HTTPC_STRICT_FOLLOW_REDIRECTS 1
#define HTTPC_DISABLE_FOLLOW_REDIRECTS 0

class HTTPClient {
public:
    bool begin(WiFiClient & /*client*/, const char * /*url*/) {
        return false;
    }
    bool begin(const char * /*url*/) {
        return false;
    }
    void end() {}
    void setTimeout(int) {}
    void setAuthorization(const char *, const char *) {}
    void addHeader(const char *, const char *) {}
    void setReuse(bool) {}
    void setFollowRedirects(int) {}
    void collectHeaders(const char * /*headerKeys*/[], size_t /*count*/) {}
    bool connected() {
        return false;
    }
    int GET() {
        return -1;
    }
    int POST(const char *) {
        return -1;
    }
    String getString() {
        return String();
    }
    String header(const char * /*name*/) {
        return String();
    }
    int getSize() {
        return 0;
    }
    WiFiClient *getStreamPtr() {
        return nullptr;
    }
    WiFiClient &getStream() {
        return stream;
    }

private:
    WiFiClient stream;
};
