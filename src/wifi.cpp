//#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "settings.h"

//connectWifi////////////////////////////////////////////////////////////////////////////////////////////////
bool beginWiFi() {if (WiFi.begin() == 0){return false;}return true;}


void connectWiFi() {
  bool isConnected = false;
  if (WiFi.status() == WL_CONNECTED && !isConnected) return;
  //Manual Wifi
  Serial.printf("Connecting to WiFi %s/%s", WIFI_SSID.c_str(), WIFI_PASS.c_str());
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.hostname(WIFI_HOSTNAME);
  WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    isConnected = false;
  }
  
  Serial.println("connected.");
  digitalWrite(LED_BUILTIN, LOW);
  isConnected = true;
}
//connectWifi////////////////////////////////////////////////////////////////////////////////////////////////
