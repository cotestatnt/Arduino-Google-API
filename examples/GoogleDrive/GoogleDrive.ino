#include <FS.h>
#include <time.h>
#include <sys/time.h>
#include <GoogleDrive.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
DNSServer dnsServer;

// Timezone definition to get properly time from NTP server
#define MYTZ "CET-1CEST,M3.5.0,M10.5.0/3"
struct tm Time;

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
  #include <SPIFFS.h>
  #include <WiFi.h>
  #include <WiFiClientSecure.h>
  #include <WebServer.h>
  #define FILESYSTEM SPIFFS
  WiFiClientSecure client;
  WebServer server;
#endif

// WiFi setup
const char* ssid = "xxxxxxxxx";         // SSID WiFi network
const char* password = "xxxxxxxxx";     // Password  WiFi network
const char* hostname = "gapi_esp";      // http://gapi_esp.local/

// Google API OAuth2.0 client setup default values (you can change later with setup webpage)
const char* client_id     =  "xxxxxxxxxxxx-xxxxxxxxxxxxxxxxxxxxxxxx.apps.googleusercontent.com";
const char* client_secret =  "xxxxxxxxxxxxxxxxxxxxx";
const char* api_key       =  "xxxxxxxxxxx-xxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
const char* scopes        =  "https://www.googleapis.com/auth/drive.file";
const char* redirect_uri  =  "https://xxxxxxxxxxxxxx.m.pipedream.net";

const char* APP_FOLDERNAME = "DataFolder";   // Google Drive online folder name
const char* dataFolderName = "/data";        // Local folder for store data files

GoogleFilelist driveList;
GoogleDriveAPI myDrive(FILESYSTEM, client, &driveList );
GapiServer myWebServer(FILESYSTEM, server);
bool apMode = true;
bool runWebServer = false;

#define MAX_NAME_LEN 16
char dataFileName[MAX_NAME_LEN];          // ex. "20201025.txt"

////////////////////////////////  NTP Time  /////////////////////////////////////////
void getUpdatedtime(const uint32_t timeout) {
    uint32_t start = millis();
    do {
        time_t now = time(nullptr);
        Time = *localtime(&now);
        delay(1);
    } while(millis() - start < timeout  && Time.tm_year <= (1970 - 1900));
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

// Create new local data file on day change
void createDayFile(){
    // Create a file for each different day
    char path[10 + MAX_NAME_LEN];
    snprintf(path, 30, "%s/%s%c", dataFolderName, dataFileName, '\0' );

    if( !FILESYSTEM.exists(dataFolderName)){
        FILESYSTEM.mkdir(dataFolderName);
    }
    // Storage of a complete day data need at least 32Kb  (one measure once a minute)
    File dataFile =  FILESYSTEM.open(path, "w");
    if (!dataFile) {
        Serial.print(F("There was an error opening the file for writing - "));
        Serial.println(path);
    }
    dataFile.close();
}

// Append new row to local data file
void appendMeasurement(){
    getUpdatedtime(0);
    uint32_t free;
    uint16_t max;
 #ifdef ESP32
    free = heap_caps_get_free_size(0);
    max = heap_caps_get_largest_free_block(0);
#elif defined(ESP8266)
    ESP.getHeapStats(&free, &max, nullptr);
#endif

    // Update name of the data file if necessary
    snprintf(dataFileName, MAX_NAME_LEN, "%04d%02d%02d.txt", Time.tm_year+1900, Time.tm_mon+1, Time.tm_mday );
    char path[10 + MAX_NAME_LEN];
    snprintf(path, 30, "%s/%s", dataFolderName, dataFileName );
    if(FILESYSTEM.exists(path)){
        char dataBuf[30];
        snprintf(dataBuf, sizeof(dataBuf), "%02d:%02d:%02d; %5d; %5d",
            Time.tm_hour, Time.tm_min, Time.tm_sec, free, max);

        File file = FILESYSTEM.open(path, "a");
        file.println(dataBuf);
        file.close();
        Serial.print(F("Append new data to file: "));
        Serial.println(dataBuf);
    }
    else
        createDayFile();
}

// Upload local data file to Google Drive.
void uploadToDrive(){
    // Check if file is already in file list (local)
    String fileid = myDrive.getFileId(dataFileName);

    // Check if a file with same name is already present online and return id
    // (Google assume that file is different also if name is equal)
    if(fileid.length() < 30)
        fileid = myDrive.searchFile(dataFileName);

    String filePath = dataFolderName;
    filePath += "/";
    filePath += dataFileName;

    String uploadedId;
    if(fileid.length() > 30) {
        Serial.print(F("\n\nFile present in app folder. Call update method for "));
        Serial.println(filePath);
        uploadedId = myDrive.uploadFile(filePath, fileid);
    }
    else {
        Serial.print(F("\n\nFile not present in app folder. Call upload method for "));
        Serial.println(filePath);
        uploadedId = myDrive.uploadFile(filePath.c_str(), myDrive.getAppFolderId(), false);
    }

    if( uploadedId.length() ){
        String text = F("File uploaded to Drive with id ");
        text += uploadedId;
        server.send(200, "text/plain", text);
        Serial.println(text);
    }
    else{
        server.send(200, "text/plain", "Error");
        Serial.print(F("\nError. File not uploaded correctly."));
    }
    Serial.println(F("\n\nApp folder content:"));
    myDrive.printFileList();
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
    //Serial.setDebugOutput(true);
    // FILESYSTEM init and load optins (ssid, password, etc etc)
    startFilesystem();
    // Set Google API certificate
#ifdef ESP8266
    //client.setNoDelay(1);
    client.setSession(&session);
    client.setTrustAnchors(&certificate);
    client.setBufferSizes(1024, 2048);
    WiFi.hostname(hostname);
#elif defined(ESP32)
    client.setCACert(google_cert);
    WiFi.setHostname(hostname);
#endif
    // WiFi INIT
    startWiFi();

    if (WiFi.isConnected()) {
        // Begin Google API handling (to store tokens and configuration, filesystem must be mounted before)
        if (myDrive.begin(client_id, client_secret, scopes, api_key, redirect_uri)) {
            // Authorization OK when we have a valid token
            if (myDrive.getState() == GoogleOAuth2::GOT_TOKEN){
                Serial.print(F("\n\n-------------------------------------------------------------------------------"));
                Serial.print(F("\nYour application has the credentials to use the google API in the selected scope\n"));
                Serial.print(F("\n---------------------------------------------------------------------------------\n\n"));
                String appFolderId = myDrive.searchFile(APP_FOLDERNAME);

                if (appFolderId != "") {
                    // A valid Google id founded
                    Serial.print("App folder id: ");
                    Serial.println(appFolderId);
                    Serial.println("\nFolder APP found, file created with this app:");
                    myDrive.updateFileList();
                    myDrive.printFileList();
                }
                else {
                    Serial.println("\nFolder APP not found. Now it will be created. ");
                    appFolderId = myDrive.createFolder(APP_FOLDERNAME, "root");
                    Serial.print("New app folder id: ");
                    Serial.println(appFolderId);
                }
                // Save the folder id for easy retrieve when needed
                myDrive.setAppFolderId(appFolderId);
            }
        }
        if(myDrive.getState() == GoogleOAuth2::INVALID){
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
            Serial.println("\nMDNS responder started.\nOn Window you need to install service like Bonjour");
            Serial.printf("You should be able to connect with address\t http://%s.local/\n", hostname);
        }
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

    // Update name of local data file
    snprintf(dataFileName, MAX_NAME_LEN, "%04d%02d%02d.txt", Time.tm_year+1900, Time.tm_mon+1, Time.tm_mday );
    // Append to the file the first measure on reboot
    appendMeasurement();

    uploadToDrive();
}

void loop(){

    // Run webserver (just for debug, not needed after first configuration)
    if(runWebServer)
        myWebServer.run();

    // Add new measure once 5 seconds
    static uint32_t dataTime;
    if(millis() - dataTime > 10000) {
        dataTime = millis();
        appendMeasurement();
    }

    // Upload data file to Google Drive once a minute
    static uint32_t uploadTime;
    if(millis() - uploadTime > 60000) {
        uploadTime = millis();
        uploadToDrive();
        Serial.print("Upload time: ");
        Serial.println(millis() - uploadTime);
    }
}
