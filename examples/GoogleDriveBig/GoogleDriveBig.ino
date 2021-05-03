#include <FS.h>
#include <time.h>
#include <sys/time.h>
#include <GoogleDrive.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
DNSServer dnsServer;

// This example work only with ESP32

#define LED 2  // Built-in led usually

// Timezone definition to get properly time from NTP server
#define MYTZ "CET-1CEST,M3.5.0,M10.5.0/3"
struct tm Time;

// Sse FFat or SPIFFS with ESP32
#include <SPIFFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#define FILESYSTEM SPIFFS
WiFiClientSecure client;
WebServer server;


// WiFi setup
const char* ssid = "xxxxxxxxxxxx";         // SSID WiFi network
const char* password = "xxxxxxxxxxxxx";     // Password  WiFi network
const char* hostname = "gapi_esp";      // http://gapi_esp.local/

// Google API OAuth2.0 client setup default values (you can change later with setup webpage)
const char* client_id     =  "xxxxxxxxxxxxxx-xxxxxxxxxxxxxxxxxxxxx.apps.googleusercontent.com";
const char* client_secret =  "xxxxxxxxxxxx";
const char* api_key       =  "xxxxxxxxx-xxxxxxxxxxxxxxxxxxxxxxxxxx";
const char* scopes        =  "https://www.googleapis.com/auth/drive.file";
const char* redirect_uri  =  "https://xxxxxxxxxxxxxxxx.m.pipedream.net";

const char* APP_FOLDERNAME = "0_DataFolder";   // Google Drive online folder name
const char* dataFileName = "audio.wav";

GoogleFilelist driveList;
GoogleDriveAPI myDrive(FILESYSTEM, client, &driveList );
GapiServer myWebServer(FILESYSTEM, server);
bool apMode = true;
bool runWebServer = false;

// This variable can be setted to true with a webserver request
bool doUpload = true;


////////////////////////////////  NTP Time  /////////////////////////////////////////
void getUpdatedtime(const uint32_t timeout) {
    uint32_t start = millis();
    do {
        time_t now = time(nullptr);
        Time = *localtime(&now);
        delay(1);
    } while(millis() - start < timeout  && Time.tm_year <= (1970 - 1900));
}


////////////////////////////////  Filesystem  /////////////////////////////////////////
void startFilesystem(){
  // FILESYSTEM INIT
    if ( FILESYSTEM.begin(true)){
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

            WiFi.setHostname(hostname);
            configTzTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
          
            // Sync time with NTP. Blocking, but with timeout (0 == no timeout)
            getUpdatedtime(10000);
            char nowTime[30];
            strftime (nowTime, 30, "%d/%m/%Y - %X", &Time);
            Serial.printf("Synced time: %s\n", nowTime);
        }
    }
}



// Upload local data file to Google Drive.
static void uploadToDrive(void * args) {
    Serial.println("Start upload");
    uint16_t uploadTime = millis();

    // Check if file is already in file list (local)
    String fileid = myDrive.getFileId(dataFileName);

    // Check if a file with same name is already present online and return id
    // (Google assume that file is different also if name is equal)
    if(fileid.length() < 30)
        fileid = myDrive.searchFile(dataFileName);

    char filePath[30];
    strcpy(filePath, "/");
    strcat(filePath, dataFileName);

    String uploadedId;
    if(fileid.length() > 30) {
        Serial.print(F("\n\nFile present in app folder. Call update method for "));
        Serial.println(filePath);
        uploadedId = myDrive.uploadFile(filePath, fileid.c_str());
    }
    else {
        Serial.print(F("\n\nFile not present in app folder. Call upload method for "));
        Serial.println(filePath);
        uploadedId = myDrive.uploadFile(filePath, myDrive.getAppFolderId(), false);
    }
    Serial.printf("Upload time: %u ms\n", millis() - uploadTime);

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

    // Delete this task on exit
    vTaskDelete(NULL);
}



void setup(){
    pinMode(LED, OUTPUT);
    Serial.begin(115200);
    //Serial.setDebugOutput(true);
    // FILESYSTEM init and load optins (ssid, password, etc etc)
    startFilesystem();
    // Set Google API certificate
    client.setCACert(google_cert);
    WiFi.setHostname(hostname);
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

        // Add custom handler to webserver
        server.on("/upload", HTTP_GET, [](){
            doUpload = true;
        });

        myWebServer.begin();
        while(apMode) {
            dnsServer.processNextRequest();
            myWebServer.run();
            yield();
            apMode = WiFi.status() != WL_CONNECTED;
        }
        dnsServer.stop();
    }

}

void loop(){

    // Run webserver (just for debug, not needed after first configuration)
    if(runWebServer)
        myWebServer.run();

    // Upload data file to Google Drive on request
    if(doUpload) {
        doUpload = false;
        //uploadToDrive();

        // Start uploading in a dedicated task
        xTaskCreate(
            uploadToDrive,    // Function to implement the task
            "uploadToDrive",  // Name of the task
            8192,             // Stack size in words
            NULL,             // Task input parameter
            1,                // Priority of the task
            NULL              // Task handler.
        );
    }

    static uint32_t blinkTime;
    if(millis() - blinkTime > 250) {
        blinkTime = millis();
        digitalWrite(LED, !digitalRead(LED));
    }

}
