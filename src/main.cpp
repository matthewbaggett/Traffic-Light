
#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>

const char* ssid = "Plugnet";
const char* password = "UntilPrinciplePlasticEgg";
const char* update_password = "bywuQ11D0GZl";
const char* mqtt_server = "10.188.5.1";
const char* watchedTopic = "traffic_light";
const char* statusTopic = "status";

int led_pin = 14;
#define N_DIMMERS 3
int led_red = 16;
int led_amber = 14;
int led_green = 12;
int dimmer_pin[] = {led_red, led_amber, led_green};

WiFiClient espClient;
PubSubClient client(espClient);

long lastMsg = 0;
char msg[50];
int value = 0;

char NRValue[8];

void lightRedOn() {
  analogWrite(led_red, 0);
  client.publish(statusTopic, "STATUS:RED:ON");
}
void lightRedOff() {
  analogWrite(led_red, 1024);
  client.publish(statusTopic, "STATUS:RED:OFF");
}

void lightAmberOn() {
  analogWrite(led_amber, 0);
  client.publish(statusTopic, "STATUS:AMBER:ON");
}
void lightAmberOff() {
  analogWrite(led_amber, 1024);
  client.publish(statusTopic, "STATUS:AMBER:OFF");
}

void lightGreenOn() {
  analogWrite(led_green, 0);
  client.publish(statusTopic, "STATUS:GREEN:ON");
}
void lightGreenOff() {
  analogWrite(led_green, 1024);
  client.publish(statusTopic, "STATUS:GREEN:OFF");
}

void allOff(){
  lightRedOff();
  lightAmberOff();
  lightGreenOff();
}

void allOn(){
  lightRedOn();
  lightAmberOn();
  lightGreenOn();
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  String myTopic = String(topic);
  String myPayload;
  for (int i = 0; i < length; i++) {
    myPayload += (char)payload[i];
  }
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(myPayload);

  if (strcmp(topic, watchedTopic) == 0) {
    if (myPayload.equals("red_off")) {
      lightRedOff();
    } else if (myPayload.equals("red_on")) {
      lightRedOn();
    } else if (myPayload.equals("amber_on")) {
      lightAmberOn();
    } else if (myPayload.equals("amber_off")) {
      lightAmberOff();
    } else if (myPayload.equals("green_on")) {
      lightGreenOn();
    } else if (myPayload.equals("green_off")) {
      lightGreenOff();
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection to ");
    Serial.print(mqtt_server);
    Serial.print(" ... ");
    // Attempt to connect
    if (client.connect(NRValue)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(statusTopic, "online");
      // ... and resubscribe
      client.subscribe(watchedTopic);
      Serial.print("Subscribed to ");
      Serial.println(watchedTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.print("ID: ");
  Serial.println(ESP.getChipId());
  String(ESP.getChipId()).toCharArray(NRValue, 8);

  Serial.print("Device Name: ");
  Serial.println(NRValue);

  /* switch on led */
  pinMode(led_pin, OUTPUT);
  digitalWrite(led_pin, LOW);

  Serial.println("Booting");

  uint32_t realSize = ESP.getFlashChipRealSize();
  uint32_t ideSize = ESP.getFlashChipSize();
  FlashMode_t ideMode = ESP.getFlashChipMode();

  Serial.printf("Flash real id:   %08X\n", ESP.getFlashChipId());
  Serial.printf("Flash real size: %u\n\n", realSize);

  Serial.printf("Flash ide  size: %u\n", ideSize);
  Serial.printf("Flash ide speed: %u\n", ESP.getFlashChipSpeed());
  Serial.printf("Flash ide mode:  %s\n", (ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN"));

  if (ideSize != realSize) {
    Serial.println("Flash Chip configuration wrong!\n");
  } else {
    Serial.println("Flash Chip configuration ok.\n");
  }

  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid, password);

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
    Serial.println("Retrying connection...");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  // Switch off LED.
  digitalWrite(led_pin, HIGH);

  // Set up MQTT.
  client.setServer(mqtt_server, 1883);
  client.setCallback(mqtt_callback);

  // configure dimmers, and OTA server events
  analogWriteRange(1000);
  analogWrite(led_pin, 990);

  for (int i = 0; i < N_DIMMERS; i++)
  {
    pinMode(dimmer_pin[i], OUTPUT);
    analogWrite(dimmer_pin[i], 50);
  }

  char deviceName[17];
  char prefix[8] = "Traffic";
  sprintf(deviceName, "%s_%s", prefix, NRValue);

  ArduinoOTA.setHostname(deviceName);
  ArduinoOTA.setPassword(update_password);

  ArduinoOTA.onStart([]() { // switch off all the PWMs during upgrade
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
    allOff();
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    //client.publish(statusTopic, "update:error");
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
    (void)error;
    ESP.restart();
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    int percent = (progress / (total / 100));
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    if(percent > 0 && percent < 33){
      lightRedOn();
    }
    if(percent >= 33 && percent < 66){
      lightRedOn();
      lightAmberOn();
    }
    if(percent >= 66 && percent < 100){
      lightRedOn();
      lightAmberOn();
      lightGreenOn();
    }
    
  });

  ArduinoOTA.onEnd([]() { 
    Serial.println("\nEnd");
    for (int i=0; i < 10; i++){
      allOff();
      delay(100);
      allOn();
      delay(100);
    }
    allOff();
  });

  /* setup the OTA server */
  ArduinoOTA.begin();
  Serial.println("Ready");
  lightRedOff();
  lightAmberOff();
  lightGreenOff();
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 10000) {
    lastMsg = now;
    client.publish(statusTopic, "PING");
  }
  ArduinoOTA.handle();
}
