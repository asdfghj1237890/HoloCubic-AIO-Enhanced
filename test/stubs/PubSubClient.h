#ifndef AIO_STUB_PUBSUBCLIENT_H
#define AIO_STUB_PUBSUBCLIENT_H
#include "Arduino.h"
#include "WiFiClient.h"

typedef void (*MQTT_CALLBACK_SIGNATURE)(char *topic, uint8_t *payload, unsigned int length);

class PubSubClient {
public:
    PubSubClient() {}
    PubSubClient(WiFiClient &) {}
    PubSubClient &setServer(const char *, uint16_t) { return *this; }
    PubSubClient &setServer(uint32_t, uint16_t) { return *this; }
    PubSubClient &setCallback(MQTT_CALLBACK_SIGNATURE) { return *this; }
    PubSubClient &setClient(WiFiClient &) { return *this; }
    PubSubClient &setBufferSize(uint16_t) { return *this; }
    bool connect(const char *) { return false; }
    bool connect(const char *, const char *, const char *) { return false; }
    void disconnect() {}
    bool publish(const char *, const char *) { return false; }
    bool publish(const char *, const uint8_t *, unsigned int, bool = false) { return false; }
    bool subscribe(const char *) { return false; }
    bool subscribe(const char *, uint8_t) { return false; }
    bool unsubscribe(const char *) { return false; }
    bool loop() { return false; }
    bool connected() { return false; }
    int state() { return -1; }
};

#endif
