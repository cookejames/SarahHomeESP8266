#include "SarahHome.h"

KeyValueStore kvstore;

SarahHome::SarahHome(String type) {
  mqttClientNameFormat = "sarah-%d";
  deviceType = type;
  mqttSubscriptionsLength = 0;
  lastTime = 0;
}

boolean SarahHome::timer(int time) {
  int now = millis();
  if (now - lastTime > time) {
    lastTime = now;
    return true;
  }
  return false;
}

/**
 * Subscribe to MQTT topics, resubscribes on reconnects
 */
boolean SarahHome::subscribe(const char* topic) {
  if (mqttSubscriptionsLength >= 20) {
    return false;
  }

  mqttSubscriptions[mqttSubscriptionsLength] = topic;
  mqttSubscriptionsLength++;
  return mqttClient.subscribe(topic);
}

String SarahHome::readString(bool output) {
  char byte = 0;
  String string = "";
  while (true) {
    if (Serial.available() > 0) {
      byte = Serial.read();
      if (output) {
        Serial.print(byte);
      }
      if (byte == '\n') {
        string.trim();
        return string;
      }
      string += byte;
    }
  }
}

String SarahHome::readString() {
  return readString(false);
}

bool SarahHome::userConfirm() {
  while (true) {
    if (Serial.available() > 0) {
      return Serial.read() == 'y';
    }
  }
}

void SarahHome::enterUserValue(String key) {
  Serial.printf("Set '%s': \n", key.c_str());
  String input = readString(true);

  if (input.length() == 0) {
    Serial.println("Skipping...");
    return;
  }

  Serial.print("You entered ");
  Serial.println(input);
  Serial.println("Confirm (y/n)?");
  if (userConfirm()) {
    kvstore.write(key, input);
  }
}

void SarahHome::changeVariables() {
  Serial.println("Clear local storage (y/n)?");
  if (userConfirm()) {
    kvstore.clear();
    Serial.println("Storage cleared");
  } else {
    Serial.println("Storage contents:");
    kvstore.output();
  }

  String keys[6] = {"mqttServer", "mqttUsername", "mqttPassword", "wifiSsid", "wifiPassword", "nodeId"};
  for (int i = 0; i < 6; i++) {
    enterUserValue(keys[i]);
  }
  Serial.println("Storage is now set to:");
  kvstore.output();
}

void SarahHome::setupKeyValueStore() {
  int timeout = millis() + 5000;
  Serial.println("Do you want to change settings? (y/n)");
  while(millis() < timeout) {
    if (Serial.available() > 0) {
      char c = Serial.read();
      if (c == 'y') {
        return changeVariables();
      } else if (c == 'n') {
        break;
      }
    }
  }
  Serial.println("Skipping settings change...");
}

void SarahHome::setupVariables() {
  mqttUsername = kvstore.read("mqttUsername");
  mqttPassword = kvstore.read("mqttPassword");
  mqttServer = kvstore.read("mqttServer");

  wifiSsid = kvstore.read("wifiSsid");
  wifiPassword = kvstore.read("wifiPassword");

  nodeId = kvstore.read("nodeId");
}

String SarahHome::getNodeId() {
  return nodeId;
}

String SarahHome::getDeviceType() {
  return deviceType;
}

void SarahHome::setup(char const* version) {
  applicationVersion = version;
  setupKeyValueStore();
  setupVariables();
  connectWifi();
  setupMqtt();
  connectMqtt();
  setupOTA();
  Serial.printf("Node %s ready\n", nodeId.c_str());
}

void SarahHome::setup() {
  setup("na");
}

void SarahHome::setupOTA() {
  ArduinoOTA.setHostname(nodeId.c_str());

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\n", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

void SarahHome::connectWifi() {
  WiFi.mode(WIFI_STA);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.printf("Attempting to connect to WPA SSID: %s\n", wifiSsid.c_str());
    // Connect to WPA/WPA2 network:
    WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());

    // wait 10 seconds for connection:
    delay(10000);

    attempts++;
    if (attempts >= 5) {
      Serial.println("Failed to connnected to Wifi after 5 attempts, restarting...");
      ESP.restart();
    }
  }

  Serial.println("**WiFi connected**");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.printf("Signal strength: %d\n", WiFi.RSSI());
}

void SarahHome::setupMqtt() {
  randomSeed(RANDOM_REG32);
  sprintf(mqttClientName, mqttClientNameFormat.c_str(), random(1000));
  mqttClient = PubSubClient(wifiClient);
  mqttClient.setServer(mqttServer.c_str(), 1883);
}

/**
 * Subscribe to all MQTT topics
 */
void SarahHome::subscribeToTopics() {
  for (int i = 0; i < mqttSubscriptionsLength; i++) {
    mqttClient.subscribe(mqttSubscriptions[i]);
  }
}

void SarahHome::connectMqtt() {
  int attempts = 0;
  char connectTopic[100];
  sprintf(connectTopic, "%s/%s/connected", deviceType.c_str(), nodeId.c_str());
  while (!mqttClient.connected()) {
    Serial.printf("MQTT client name: %s\n", mqttClientName);
    Serial.printf("Attempting to connect to MQTT server: %s\n", mqttServer.c_str());
    boolean connected = mqttClient.connect(
      mqttClientName,
      mqttUsername.c_str(),
      mqttPassword.c_str(),
      connectTopic,
      0,
      1,
      "false"
    );
    if (!connected) {
      Serial.print("MQTT Connection Error: ");
      Serial.println(mqttClient.state());

      attempts++;
      if (attempts >=5) {
        Serial.println("Failed to connnected to MQTT after 5 attempts, restarting...");
        ESP.restart();
      }
      delay(10000);
    }
  }

  Serial.println("**MQTT Connected**");
  mqttClient.publish(connectTopic, "true", true);

  char versionTopic[100];
  sprintf(connectTopic, "%s/%s/version", deviceType.c_str(), nodeId.c_str());
  mqttClient.publish(connectTopic, applicationVersion, true);
  subscribeToTopics();
}

void SarahHome::loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWifi();
  }
  ArduinoOTA.handle();
  if (!mqttClient.loop()) {
    connectMqtt();
  }
}
