#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <DNSServer.h>
#include <MQTTClient.h>
#include "WiFiManager.h"          //https://github.com/tzapu/WiFiManager
#include <ESP8266WebServer.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <Ticker.h>

#define WM_TIMEOUT 120
#define tickerCycleTime 30
//define the pinout, equaals the labels used on the board.
#define OUT 4 //pad is connected by resistor to the pin on the chip
#define SEL D0
#define R 5
#define G 14
#define B 12
#define W 13

File configFile;
char host[40] = "broker.shiftr.io";
char user[40] = "try";
char password[40] = "try";
char clientID[40] = "clientID";
String nodeName = "clientID";

bool shouldSaveConfig = false;

WiFiClient wifi;
MQTTClient mqtt;

Ticker handleFan0;
Ticker handleFan1;
Ticker handleFan2;
Ticker handleFan3;

//pin order in my case B, R, W, G
uint8_t pins[4] = {12, 5, 13, 14};
int currentPWM[4] = {0, 0, 0, 0};
int targetPWM[4] = {0, 0, 0, 0};

void setPWM(int pin) {
  if (currentPWM[pin] > targetPWM[pin]) {
    --currentPWM[pin];
    analogWrite(pins[pin], currentPWM[pin]);
  } else if (currentPWM[pin] < targetPWM[pin]) {
    ++currentPWM[pin];
    analogWrite(pins[pin], currentPWM[pin]);
  } else {
    if (pin == 0) handleFan0.detach();
    else if (pin == 1) handleFan1.detach();
    else if (pin == 2) handleFan2.detach();
    else if (pin == 3) handleFan3.detach();
  }
}


void delayed(int pin) {
  if (pin == 0) handleFan0.attach_ms(tickerCycleTime, setPWM, pin);
    else if (pin == 1) handleFan1.attach_ms(tickerCycleTime, setPWM, pin);
    else if (pin == 2) handleFan2.attach_ms(tickerCycleTime, setPWM, pin);
    else if (pin == 3) handleFan3.attach_ms(tickerCycleTime, setPWM, pin);
  
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Booting...");
  loadConfig();
  useWifiManager();
  saveConfig();
  analogWriteFreq(16000);
  analogWriteRange(100);

  nodeName=clientID;

  connectWifi();

  mqtt.begin(host, wifi);
  Serial.print("Start MQTT: ");
  Serial.println(host);

}

void loop() {
  mqtt.loop();

  if (!mqtt.connected()) {
    connect();
  }

  mqtt.publish("/"+nodeName+"/currentPWM", String(currentPWM[0]));

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
  mqtt.subscribe("/"+nodeName+"/fan/#");
  
  mqtt.publish("/"+nodeName+"/ip", String(WiFi.localIP()[0]) + "." + String(WiFi.localIP()[1]) + "." + String(WiFi.localIP()[2]) + "." + String(WiFi.localIP()[3]));
}

void connectWifi() {
  Serial.print("checking wifi... ");
  Serial.print(WiFi.SSID());

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
  wifiManager.setTimeout(WM_TIMEOUT);

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

  if (topic == "/"+nodeName+"/fan/all") {
    int value = payload.toInt();
    for (int i = 0; i <= 3; i++) {
      targetPWM[i] = value;
    }
    handleFan0.attach_ms(tickerCycleTime, setPWM, 0);
    delay(10);
    handleFan1.once_ms(1000, delayed, 1);
    delay(10);
    handleFan2.once_ms(2000, delayed, 2);
    delay(10);
    handleFan3.once_ms(3000, delayed, 3);
  } else if (topic == "/"+nodeName+"/fan/0") {
    targetPWM[0] = payload.toInt();
    handleFan0.attach_ms(tickerCycleTime, setPWM, 0);
  } else if (topic == "/"+nodeName+"/fan/1") {
    targetPWM[1] = payload.toInt();
    handleFan1.attach_ms(tickerCycleTime, setPWM, 1);
  } else if (topic == "/"+nodeName+"/fan/2") {
    targetPWM[2] = payload.toInt();
    handleFan2.attach_ms(tickerCycleTime, setPWM, 2);
  } else if (topic == "/"+nodeName+"/fan/3") {
    targetPWM[3] = payload.toInt();
    handleFan3.attach_ms(tickerCycleTime, setPWM, 3);
  }
  
}

