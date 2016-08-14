#ifndef SarahHome_h
#define SarahHome_h

#include "Arduino.h"
#include "ESP8266WiFi.h"
#include "ESP8266mDNS.h"
#include "WiFiUdp.h"
#include "ArduinoOTA.h"
#include "PubSubClient.h"
#include "KeyValueStore.h"

class SarahHome
{
  public:
    SarahHome(String);
    void setup(char const*);
    void setup();
    void loop();
    String getNodeId();
    String getDeviceType();
    PubSubClient mqttClient;
  private:
    void setupVariables();
    void setMqttName();
    void connectWifi();
    void connectMqtt();
    void setupOTA();
    void changeVariables();
    String readString(bool);
    String readString();
    bool userConfirm();
    void enterUserValue(String);
    void setupKeyValueStore();

    String mqttUsername;
    String mqttPassword;
    String mqttServer;
    String mqttClientNameFormat;
    char mqttClientName[50];
    String deviceType;
    WiFiClient wifiClient;
    String wifiSsid;
    String wifiPassword;
    String nodeId;
    char const* applicationVersion;
};
#endif
