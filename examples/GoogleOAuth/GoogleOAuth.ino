#include <FS.h>
#include <time.h>
#include <sys/time.h>
#include <GoogleOAuth2.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
DNSServer dnsServer;

#ifdef ESP8266
  #include <LittleFS.h>  
  #include <ESP8266WiFi.h>
  #include <WiFiClient.h>
  #include <ESP8266WebServer.h>
  ESP8266WebServer server(80);
  BearSSL::WiFiClientSecure client;
  BearSSL::Session   session;
  BearSSL::X509List  certificate(google_cert);
  #define FILESYSTEM LittleFS
#elif defined(ESP32)
  // Sse FFat or SPIFFS with ESP32
  #include <FFat.h>
  #include <WebServer.h>    
  #define FILESYSTEM SPIFFS
  WiFiClientSecure client;
  WebServer server;
#endif

// WiFi setup
const char* ssid = "xxxxxxxx";         // SSID WiFi network
const char* password = "xxxxxxxxx";     // Password  WiFi network
const char* hostname = "gapi_esp";     // http://gapi_esp.local/

// Google API OAuth2.0 client setup default values (you can change later with setup webpage)
// Google API OAuth2.0 client setup default values (you can change later with setup webpage)
const char* client_id     =  "xxxxxxxxxxxx-f9g6btf4ip5gexxxxxxxxq01qvoa2srhf.apps.googleusercontent.com";
const char* client_secret =  "xxxxxxxxxxxxxxxxxxxxxxxx";
const char* api_key       =  "xxxxxx-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
const char* scopes        =  "profile";
const char* redirect_uri  =  "https://XXXXXXXXXXXXXXXX.m.pipedream.net";

// Timezone definition to get properly time from NTP server
#define MYTZ "CET-1CEST,M3.5.0,M10.5.0/3"
struct tm Time;

GoogleOAuth2 myGoogle(FILESYSTEM, client );
GapiServer myWebServer(FILESYSTEM, server);
bool apMode = true;
bool runWebServer = false;

void customPageHandler(){
    Serial.println("Custom handler");
    server.send(200, "text/json", "Custom handler");
}

////////////////////////////////  Filesystem  /////////////////////////////////////////
void startFilesystem(){
  // FILESYSTEM INIT
    if ( FILESYSTEM.begin()){
        File root = FILESYSTEM.open("/", "r");
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

////////////////////////////////  NTP Time  /////////////////////////////////////////
void getUpdatedtime(const uint32_t timeout)
{
    uint32_t start = millis();
    Serial.print("Sync time...");
    do {
        time_t now = time(nullptr);
        Time = *localtime(&now);
        delay(1);
    } while(millis() - start < timeout  && Time.tm_year <= (1970 - 1900));
    Serial.println(" done.");
}

////////////////////////////////  Heap memory info  /////////////////////////////////
void printHeapStats() {
  static uint32_t heapTime;
  if (millis() - heapTime > 10000) {
    heapTime = millis();
    time_t now = time(nullptr);
    Time = *localtime(&now);
#ifdef ESP32
    Serial.printf("%02d:%02d:%02d - Total free: %6d - Max block: %6d\n",
      Time.tm_hour, Time.tm_min, Time.tm_sec, heap_caps_get_free_size(0), heap_caps_get_largest_free_block(0) );
#elif defined(ESP8266)
    uint32_t free;
    uint16_t max;
    ESP.getHeapStats(&free, &max, nullptr);
    Serial.printf("%02d:%02d:%02d - Total free: %5d - Max block: %5d\n",
      Time.tm_hour, Time.tm_min, Time.tm_sec, free, max);
#endif
  }
}

////////////////////////////////  WiFi  /////////////////////////////////////////
void startWiFi(){
    if(ssid != nullptr && password != nullptr) {
        Serial.printf("Connecting to %s\n", ssid);
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, password);
        // Try to connect to ssid, if it fail go back to AP_MODE
        uint32_t startTime = millis();
        while (WiFi.status() != WL_CONNECTED ){
            delay(500);
            Serial.print(".");
            if( millis() - startTime > 20000 )
                break;
        }
        if(WiFi.status() == WL_CONNECTED) {
            apMode = false;
            Serial.print("\nConnected! IP address: ");
            Serial.println(WiFi.localIP());
            // Set hostname, timezone and NTP servers
        #ifdef ESP8266
            WiFi.hostname(hostname);
            configTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
        #elif defined(ESP32)
            WiFi.setHostname(hostname);
            configTzTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
        #endif
            // Sync time with NTP. Blocking, but with timeout (0 == no timeout)
            getUpdatedtime(10000);
            char nowTime[30];
            strftime (nowTime, 30, "%d/%m/%Y - %X", &Time);
            Serial.printf("Synced time: %s\n", nowTime);
        }
    }
}

void setup(){
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    // FILESYSTEM init and load optins (ssid, password, etc etc)
    startFilesystem();
    // Set Google API certificate
#ifdef ESP8266
    client.setSession(&session);
    client.setTrustAnchors(&certificate);
    client.setBufferSizes(1024, 1024);
    WiFi.hostname(hostname);
#elif defined(ESP32)
    client.setCACert(google_cert);
    WiFi.setHostname(hostname);
#endif
    // WiFi INIT
    startWiFi();

    if (WiFi.isConnected()) {
        // Begin Google API handling (to store tokens and configuration, filesystem must be mounted before)
        if (myGoogle.begin(client_id, client_secret, scopes, api_key, redirect_uri)){
            // Authorization OK when we have a valid token
            if (myGoogle.getState() == GoogleOAuth2::GOT_TOKEN){
                Serial.print(F("\n\n-------------------------------------------------------------------------------"));
                Serial.print(F("\nYour application has the credentials to use the google API in the selected scope\n"));
                Serial.print(F("\n---------------------------------------------------------------------------------\n\n"));
            }
        }
        if(myGoogle.getState() == GoogleOAuth2::INVALID){
            Serial.print(F("\n\n-------------------------------------------------------------------------------"));
            Serial.print(F("\nGoogle says that your client is NOT VALID! You have to authorize the application."));
            Serial.print(F("\nFor instructions, check the page https://github.com/cotestatnt/Arduino-Google-API\n"));
            Serial.print(F("\nOpen this link with your browser to authorize this appplication, then restart.\n\n"));
            Serial.printf("\nhttp://%s.local/\n", hostname);
            Serial.print(F("\n---------------------------------------------------------------------------------\n\n"));
            runWebServer = true;
        }
    }
    // Application must be authorized, start ESP in access point mode (captive mode) and run webserver
    runWebServer = true;        // uncomment this to test webserver even when not necessary for authorization flow
    if(apMode || runWebServer ) {
        if(apMode) {
            Serial.print("\nStart Access point mode");
            WiFi.mode(WIFI_AP);
            WiFi.softAP(hostname);
            // if DNSServer is started with "*" for domain name, it will reply with provided IP to all DNS request
            dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
            Serial.printf("\ndns server started with ip: %s\n", WiFi.softAPIP().toString().c_str());
            dnsServer.start(53, F("*"), WiFi.softAPIP());
        }
        if (MDNS.begin(hostname)) {
            Serial.println("MDNS responder started.\nOn Window you need to install service like Bonjour");
            Serial.printf("You should be able to connect with address\t http://%s.local/\n", hostname);
        }

        // Add custom handler to webserver
        server.on("/test", HTTP_GET, customPageHandler);

        myWebServer.addOption(FILESYSTEM, "hostname", hostname);
        myWebServer.begin();
        while(apMode) {
            dnsServer.processNextRequest();
            myWebServer.run();
            #ifdef ESP8266
            MDNS.update();
            #endif
            yield();
            apMode = WiFi.status() != WL_CONNECTED;
        }
        dnsServer.stop();
    }

}

void loop(){
    if(runWebServer)
        myWebServer.run();

    printHeapStats();
}