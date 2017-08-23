#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <MQTTClient.h>
#include "WiFiManager.h"          //https://github.com/tzapu/WiFiManager
#include <ESP8266WebServer.h>
#include <FS.h> 
#include <ArduinoJson.h>

//define the pinout, equaals the labels used on the board.
#define OUT D5 //pad is connected by resistor to the pin on the chip
#define SEL D0
#define R D4
#define G D14
#define B D12
#define W D13

File configFile;
String host = "broker.shifter.io";
String user = "try";
String password = "try";
String clientID = "clientID";


bool shouldSaveConfig = false;

WiFiClient wifi;
MQTTClient mqtt;

void setup() {
  Serial.begin(115200);
  loadConfig();
  useWifiManager();
  saveConfig();
  
  mqtt.begin(toCharArray(host), wifi);
  Serial.println("Start MQTT");
}

void loop() {
  mqtt.loop();
  delay(10);
  if (!mqtt.connected()) {
    connect();
  }

  delay(500);
}

void connect() {
  Serial.print("checking wifi...");
  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nconnected!");

  Serial.print("checking mqtt...");
  while (!mqtt.connect(toCharArray(clientID), toCharArray(user), toCharArray(password))) {
    Serial.print(".");
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

          char hostTmp[40];
          strcpy(hostTmp, json["host"]);
          host = String(hostTmp);

          char userTmp[40];
          strcpy(userTmp, json["user"]);
          user = String(userTmp);

          char passwordTmp[40];
          strcpy(passwordTmp, json["password"]);
          password = String(passwordTmp);

          char clientIDTmp[40];
          strcpy(clientIDTmp, json["clientID"]);
          clientID = String(clientIDTmp);

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
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", toCharArray(host), 40);
  WiFiManagerParameter custom_mqtt_user("user", "mqtt user", toCharArray(user), 40);
  WiFiManagerParameter custom_mqtt_pass("password", "mqtt password", toCharArray(password), 40);
  WiFiManagerParameter custom_mqtt_clientID("clientID", "mqtt clientID", toCharArray(clientID), 40);
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  wifiManager.setTimeout(180);

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
    if(!wifiManager.autoConnect("ESP8266_CONFIGMODE")) {
      Serial.println("failed to connect and hit timeout");
  
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(1000);
    }
  }

  host = String(custom_mqtt_server.getValue());
  //strcpy(host, custom_mqtt_server.getValue());
  Serial.print("mqtt server: ");
  Serial.println(host);

  user = String(custom_mqtt_user.getValue());
  //strcpy(user, custom_mqtt_user.getValue());
  Serial.print("mqtt user: ");
  Serial.println(user);

  password = String(custom_mqtt_pass.getValue());
  //strcpy(password, custom_mqtt_pass.getValue());
  Serial.print("mqtt password: ");
  Serial.println(password);

  clientID = String(custom_mqtt_clientID.getValue());
  //strcpy(clientID, custom_mqtt_clientID.getValue());
  Serial.print("mqtt clientID: ");
  Serial.println(clientID);
}

void messageReceived(String topic, String payload, char * bytes, unsigned int length) {
  Serial.print("incoming: ");
  Serial.print(topic);
  Serial.print(" - ");
  Serial.print(payload);
  Serial.println();
}

