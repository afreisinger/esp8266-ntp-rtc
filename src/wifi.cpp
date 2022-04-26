#include <ESP8266WiFi.h>

//connectWifi////////////////////////////////////////////////////////////////////////////////////////////////
bool beginWiFi() 
{
  if (WiFi.begin() == 0){return false;} return true;
}

void connectWiFi(const char* WIFI_SSID,const char* WIFI_PASS, const char* WIFI_HOSTNAME) 
{
  bool isConnected = false;
  if (WiFi.status() == WL_CONNECTED && !isConnected) return;
  
  Serial.printf("Connecting to WiFi %s/%s", WIFI_SSID, WIFI_PASS);
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.hostname(WIFI_HOSTNAME);
  
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) 
      {
        delay(1000);
        Serial.print(F("."));
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        isConnected = false;
      };
  Serial.printf("\nSsuccessfully connected !!!\nIP address: %s/%s\n", 
                WiFi.localIP().toString().c_str(), 
                WiFi.subnetMask().toString().c_str()
                ); //Get ip and subnet mask
  Serial.printf("MAC address: %s\n\n", WiFi.macAddress().c_str());  //Get the local mac 
};
//connectWifi////////////////////////////////////////////////////////////////////////////////////////////////
