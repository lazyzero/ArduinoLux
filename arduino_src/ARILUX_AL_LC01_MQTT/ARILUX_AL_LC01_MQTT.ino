#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <MQTTClient.h>
#include "WiFiManager.h"          //https://github.com/tzapu/WiFiManager
#include <ESP8266WebServer.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <Ticker.h>

//define the pinout, equaals the labels used on the board.
#define OUT D5 //pad is connected by resistor to the pin on the chip
#define SEL D0
#define R D4
#define G D14
#define B D12
#define W D13

File configFile;
char host[40] = "broker.shiftr.io";
char user[40] = "try";
char password[40] = "try";
char clientID[40] = "clientID";
String nodeName = "clientID";

bool shouldSaveConfig = false;

WiFiClient wifi;
MQTTClient mqtt;

Ticker handleFan;
int currentPWM = 0;

void setPWM(int pwm) {
  
  if (currentPWM > pwm) {
    analogWrite(R, --currentPWM);
  } else if (currentPWM < pwm) {
    analogWrite(R, ++currentPWM);
  } else {
    handleFan.detach();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");
  loadConfig();
  useWifiManager();
  saveConfig();

  nodeName=clientID;

  connectWifi();

  

//  ArduinoOTA.setHostname("fan");
//  ArduinoOTA.setPassword((const char *)"12345678");
//  ArduinoOTA.onStart([]() {
//    Serial.println("Start");
//  });
//  ArduinoOTA.onEnd([]() {
//    Serial.println("\nEnd");
//  });
//  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
//    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
//  });
//  ArduinoOTA.onError([](ota_error_t error) {
//    Serial.printf("Error[%u]: ", error);
//    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
//    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
//    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
//    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
//    else if (error == OTA_END_ERROR) Serial.println("End Failed");
//  });
//  ArduinoOTA.begin();

  mqtt.begin(host, wifi);
  Serial.print("Start MQTT: ");
  Serial.println(host);

  handleFan.attach_ms(100, setPWM, 255);
}

void loop() {
  //ArduinoOTA.handle();

  mqtt.loop();

  if (!mqtt.connected()) {
    connect();
  }

  //Serial.println(currentPWM);
  mqtt.publish("/"+nodeName+"/currentPWM", String(currentPWM));
  if (currentPWM >= 255) handleFan.attach_ms(100, setPWM, 0);
  if (currentPWM <= 0) handleFan.attach_ms(100, setPWM, 255);
  delay(1000);
}

void connect() {
  connectWifi();
  Serial.print("client: ");
  Serial.print(clientID);
  Serial.print(" user: ");
  Serial.print(user);
  Serial.print(" pass: ");
  Serial.print(password);
  Serial.print(" checking mqtt...: ");
  while (!mqtt.connect(clientID, user, password)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nconnected!");

  mqtt.publish("/"+nodeName+"/ip", String(WiFi.localIP()[0]) + "." + String(WiFi.localIP()[1]) + "." + String(WiFi.localIP()[2]) + "." + String(WiFi.localIP()[3]));
}

void connectWifi() {
  Serial.print("checking wifi...");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nconnected!");

}

char* toCharArray(String s) {
  char* asChar;
  s.trim();
  s.toCharArray(asChar, s.length());
  return asChar;
}

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void saveConfig() {
  if (shouldSaveConfig) {


    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["host"] = host;
    json["user"] = user;
    json["password"] = password;
    json["clientID"] = clientID;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
  }
}

void loadConfig() {
  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      Serial.println("reading config file");
      configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          //char hostTmp[40];
          strcpy(host, json["host"]);
          //host = String(hostTmp);

          //char userTmp[40];
          strcpy(user, json["user"]);
          //user = String(userTmp);

          //char passwordTmp[40];
          strcpy(password, json["password"]);
          //password = String(passwordTmp);

          //char clientIDTmp[40];
          strcpy(clientID, json["clientID"]);
          //clientID = String(clientIDTmp);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
}

void useWifiManager() {
  //WiFiManager
  WiFiManagerParameter custom_text("Configure your MQTT broker");
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", host, 40);
  WiFiManagerParameter custom_mqtt_user("user", "mqtt user", user, 40);
  WiFiManagerParameter custom_mqtt_pass("password", "mqtt password", password, 40);
  WiFiManagerParameter custom_mqtt_clientID("clientID", "mqtt clientID", clientID, 40);
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  wifiManager.setTimeout(120);

  //reset settings - for testing
  //wifiManager.resetSettings();

  wifiManager.addParameter(&custom_text);
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_pass);
  wifiManager.addParameter(&custom_mqtt_clientID);

  wifiManager.setSaveConfigCallback(saveConfigCallback);

  if (ESP.getResetReason() == "External System") {
    wifiManager.startConfigPortal("ESP8266_CONFIGMODE");
  } else {
    //fetches ssid and pass and tries to connect
    //if it does not connect it starts an access point with the specified name
    //here  "ESP8266_CONFIGMODE"
    //and goes into a blocking loop awaiting configuration
    if (!wifiManager.autoConnect("ESP8266_CONFIGMODE")) {
      Serial.println("failed to connect and hit timeout");

      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(1000);
    }
  }

  if (shouldSaveConfig) {
    //host = String(custom_mqtt_server.getValue());
    strcpy(host, custom_mqtt_server.getValue());
    Serial.print("mqtt server: ");
    Serial.println(host);

    //user = String(custom_mqtt_user.getValue());
    strcpy(user, custom_mqtt_user.getValue());
    Serial.print("mqtt user: ");
    Serial.println(user);

    //password = String(custom_mqtt_pass.getValue());
    strcpy(password, custom_mqtt_pass.getValue());
    Serial.print("mqtt password: ");
    Serial.println(password);

    //clientID = String(custom_mqtt_clientID.getValue().toCharArray());
    strcpy(clientID, custom_mqtt_clientID.getValue());
    Serial.print("mqtt clientID: ");
    Serial.println(clientID);
  }
}

void messageReceived(String topic, String payload, char * bytes, unsigned int length) {
  Serial.print("incoming: ");
  Serial.print(topic);
  Serial.print(" - ");
  Serial.print(payload);
  Serial.println();
}

