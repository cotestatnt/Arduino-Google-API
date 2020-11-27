/*
 Name:	      GooglDrive.ino
 Created:     26/11/2020
 Author:      Tolentino Cotesta <cotestatnt@yahoo.com>
 Description: this example it's just to check if Google Auth2.0 flow work as expected. Work with both ESP8666 and ESP32.
 
 This example include a local webserver for easy management of files and folder (it's just customized version of
 FSBrowser example included with Arduino core for ESP8266)
 The edit page http://espfs.local/edit can be optionally stored in flash via PROGMEM keyword (espWebServer.h), 
 so you can easily upload or edit your files only with a web browser (only 6Kb of flash).
 ESP32 platform could have a little bug to fix https://github.com/espressif/arduino-esp32/issues/3652
*/

#include <FS.h>
#include <GoogleOAuth2.h>

#ifdef ESP8266
  // With ESP8266 is better to use LittleFS
  #include <LittleFS.h>
  const char* fsName = "LittleFS"; 
  #define fileSystem LittleFS
  LittleFSConfig fileSystemConfig = LittleFSConfig();  
#elif defined(ESP32) 
  // LittleFS is not supported on ESP32 platform, use FFat or SPIFFS
  #include <FFat.h>
  const char* fsName = "FFat"; 
  #define fileSystem FFat
#endif

bool fsOK = false;
const char *hostname = "espfs";
#include "espWebserver.h"

#define FORMAT_IF_FAILED true   // Format filesystem if needed

// WiFi setup
const char* ssid        =  "ssid";     // SSID WiFi network
const char* password    =  "xxxx";     // Password  WiFi network    

GoogleOAuth2 myGoogle(&fileSystem, "/oauth2.json");

void setup(){
    Serial.begin(115200);
    Serial.setDebugOutput(true);

    // WiFi INIT
    startWiFi();

    // FILESYSTEM INIT
    startFilesystem();

    // Begin, after wait authorize if necessary (with 60s timeout)
    if(myGoogle.begin()){
         uint32_t timeout = millis();
        while (myGoogle.getState() != GoogleOAuth2::GOT_TOKEN){
            // Wait for user authorization on https//www.google.come/device
            delay(10000);
            if(millis() - timeout > 60000) break;
        }
        delay(1000);    
        myGoogle.printConfig();        
    }
	else{
        Serial.println("Check config file, or pass Client ID, Client Secret and App Scopes to begin()");
    }
	
	// WEB SERVER INIT
    startWebServer();  
}

void loop(){
    server.handleClient();
#ifndef ESP32
    MDNS.update();
#endif    
}


void startFilesystem(){
  // FILESYSTEM INIT
  #ifdef ESP8266
    fileSystemConfig.setAutoFormat(true);
    fileSystem.setConfig(fileSystemConfig);
    fsOK = fileSystem.begin();
  
  #elif defined(ESP32) 
    //fileSystem.format();
    fsOK = fileSystem.begin(FORMAT_IF_FAILED);
  #endif

    if (fsOK){
        File root = fileSystem.open("/", "r");
        File file = root.openNextFile();
        while (file){
            const char* fileName = file.name();
            size_t fileSize = file.size();
            Serial.printf("FS File: %s, size: %lu\n", fileName, (long unsigned)fileSize);
            file = root.openNextFile();
        }
        Serial.println();
    }
}


void startWiFi(){

#ifdef ESP8266
    WiFi.hostname(hostname);  
#elif defined(ESP32) 
    WiFi.setHostname(hostname);
#endif 
    Serial.printf("Connecting to %s\n", ssid);
    if (String(WiFi.SSID()) != String(ssid)){
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, password);
    }
    while (WiFi.status() != WL_CONNECTED){
        delay(500);
        Serial.print(".");
    }
    Serial.print("\nConnected! IP address: ");
    Serial.println(WiFi.localIP());

    MDNS.begin(hostname);
}
