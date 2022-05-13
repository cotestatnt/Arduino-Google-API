#include <GoogleSheet.h>
#include <esp-fs-webserver.h>   // https://github.com/cotestatnt/esp-fs-webserver

// Timezone definition to get properly time from NTP server
#define MYTZ "CET-1CEST,M3.5.0,M10.5.0/3"
struct tm Time;

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

#include <FS.h>
#include <LittleFS.h>
#define FILESYSTEM LittleFS

#ifdef ESP8266
ESP8266WebServer server(80);
BearSSL::WiFiClientSecure client;
BearSSL::Session   session;
BearSSL::X509List  certificate(google_cert);
#elif defined(ESP32)
WiFiClientSecure client;
WebServer server;
#endif

#include "config.h"

/// WiFi setup
String hostname = HOSTNAME;
// Remote
String appFolderName = "myDriveFolder"; // Google Drive online folder name
// Local (data filename will be date-related once for day  ex. "20201025.txt")
#define MAX_NAME_LEN 16
String dataFolderName = FOLDER_NAME;    // Local folder for store data files
char dataFilePath[strlen(FOLDER_NAME) + MAX_NAME_LEN +1];
char dataFileName[MAX_NAME_LEN + 1];    // ex. "20201025.txt"  

GoogleFilelist driveList;
GoogleDriveAPI myDrive(FILESYSTEM, client, &driveList );
FSWebServer myWebServer(FILESYSTEM, server);
bool runWebServer = true;
bool authorized = false;

// This is the webpage used for authorize the application (OAuth2.0)
#include "gaconfig_htm.h"
void handleConfigPage() {
  WebServerClass* webRequest = myWebServer.getRequest();
  webRequest->sendHeader(PSTR("Content-Encoding"), "gzip");
  webRequest->send_P(200, "text/html", AUTHPAGE_HTML, AUTHPAGE_HTML_SIZE);
}

////////////////////////////////  Application setup  /////////////////////////////////////////
void loadApplicationConfig() {
  StaticJsonDocument<2048> doc;
  File file = FILESYSTEM.open("/config.json", FILE_READ);
  if (file) {
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) {
      Serial.println(F("Failed to deserialize file, may be corrupted"));
      Serial.println(error.c_str());
      FILESYSTEM.remove("/config.json");
    }
    else {
      // serializeJsonPretty(doc, Serial);
      appFolderName = doc["Google Drive Folder Name"].as<String>();
      dataFolderName = doc["Local Folder Name"].as<String>();
    }
  }
}

////////////////////////////////  NTP Time  /////////////////////////////////////////
void getUpdatedtime(const uint32_t timeout) {
  uint32_t start = millis();
  do {
    time_t now = time(nullptr);
    Time = *localtime(&now);
    delay(1);
  } while (millis() - start < timeout  && Time.tm_year <= (1970 - 1900));
}

////////////////////////////////  Create new local data file on day change  /////////////////////////////////////////
void createDayFile(const char * filePath) {
  if (!FILESYSTEM.exists(filePath)) {
    if (strchr(filePath, '/')) {
      Serial.printf("Create missing folders of: %s\r\n", filePath);
      char *pathStr = strdup(filePath);
      if (pathStr) {
        char *ptr = strchr(pathStr, '/');
        while (ptr) {
          *ptr = 0;
          FILESYSTEM.mkdir(pathStr);
          *ptr = '/';
          ptr = strchr(ptr + 1, '/');
        }
      }
      free(pathStr);
    }
  }
  // Storage of a complete day data need at least 32Kb  (one measure once a minute)
  File file =  FILESYSTEM.open(filePath, FILE_WRITE);
  if (!file) {
    Serial.print(F("There was an error opening the file for writing - "));
    Serial.println(filePath);
  }
  file.close();
}


////////////////////////////////   Append new row to local data file /////////////////////////////////////////
void appendMeasurement() {

  getUpdatedtime(100);
  uint32_t free, max;
#ifdef ESP32
  free = heap_caps_get_free_size(0);
  max = heap_caps_get_largest_free_block(0);
#elif defined(ESP8266)
  ESP.getHeapStats(&free, &max, nullptr);
#endif

  // Create name of the data file (if it's a new day, will be created a new data file)  
  snprintf(dataFileName, MAX_NAME_LEN, "%04d%02d%02d.txt", Time.tm_year + 1900, Time.tm_mon + 1, Time.tm_mday );
  snprintf(dataFilePath, 30, "/%s/%s", dataFolderName, dataFileName);
  
  if (FILESYSTEM.exists(dataFilePath)) {
    // Add updated data to file
    char dataBuf[30];
    snprintf(dataBuf, sizeof(dataBuf), "%02d:%02d:%02d; %6lu; %6lu", Time.tm_hour, Time.tm_min, Time.tm_sec, free, max);
    File file = FILESYSTEM.open(dataFilePath, FILE_APPEND);
    file.println(dataBuf);
    file.close();
    Serial.print(F("Append new data to file: "));
    Serial.println(dataBuf);
  }
  else {
    Serial.print(F("Create new file "));
    Serial.println(dataFilePath);
    createDayFile(dataFilePath);
  }
}

////////////////////////////////  Start Filesystem  /////////////////////////////////////////
void startFilesystem() {
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
    return;
  }
  else {
    File root = FILESYSTEM.open("/", "r");
    File file = root.openNextFile();
    while (file) {
      const char* fileName = file.name();
      size_t fileSize = file.size();
      Serial.printf("FS File: %s, size: %lu\n", fileName, (long unsigned)fileSize);
      file = root.openNextFile();
    }
    Serial.println();
  }
}

////////////////////////////////  Create Google Drive folder  /////////////////////////////////////////
bool createDriveFolder() {
  if (WiFi.isConnected()) {
    // Begin Google API handling (to store tokens and configuration, filesystem must be mounted before)
    if (myDrive.begin(client_id, client_secret, scopes, api_key, redirect_uri)) {
      // Authorization OK when we have a valid token
      if (myDrive.getState() == GoogleOAuth2::GOT_TOKEN) {
        Serial.print(F("\n\n-------------------------------------------------------------------------------"));
        Serial.print(F("\nYour application has the credentials to use the google API in the selected scope\n"));
        Serial.print(F("\n---------------------------------------------------------------------------------\n\n"));
                
        String appFolderId =  myDrive.searchFile(appFolderName);  

        // A valid Google id founded
        if (appFolderId.length() >= MIN_ID_LEN ) {
          // Save the folder id for easy retrieve when needed
          myDrive.setAppFolderId(appFolderId);
          Serial.printf("App folder id: %s\n", appFolderId.c_str());
          
          if (myDrive.updateFileList()) {
            Serial.println("\n\nFile created with this app:");
            myDrive.printFileList();
          }
          else  
            Serial.println("List empty");
        }
        else {
          Serial.printf("Folder %s not present. Now it will be created.\n", appFolderName.c_str());
          appFolderId = myDrive.createFolder(appFolderName, "root");
          myDrive.setAppFolderId(appFolderId);
          Serial.printf("App folder id: %s\n", appFolderId.c_str());
        }
        return true;
      }
    }
    if (myDrive.getState() == GoogleOAuth2::INVALID) {
      Serial.print(warning_message);
      runWebServer = true;
      return false;
    }
  }
  return false;
}

////////////////////////////////  Upload local data file to Google Drive  /////////////////////////////////////////
/*
    Uploading file to Google Drive can block the MCU for several seconds.
    Let's use a FreeRTOS task to handle and keeping the MCU free to do whatever else meanwhile
*/
void uploadToDrive(void * args) {
  // Check if file is already in file list (local)
  String fileid = myDrive.getFileId(dataFileName);
  
  // Check if a file with same name is already present online and return id
  // (Google assume that file is different also if name is equal)
  if (!fileid.length())
    fileid = myDrive.searchFile(dataFileName);
  else
    Serial.printf("File id from list: %s\n", fileid.c_str());

  if (fileid.length()) {
    Serial.printf("%s - file present in app folder. \nCall update method for  \"%s\"\n", fileid.c_str(), dataFilePath);
    fileid = myDrive.uploadFile(dataFilePath, fileid.c_str(), true);
  }
  else {
    Serial.printf("\n\nFile not present in app folder. Call upload method for \"%s\"\n", dataFilePath);
    String appFolderId = myDrive.getAppFolderId();
    fileid = myDrive.uploadFile(dataFilePath, appFolderId.c_str(), false);
  }

  if (fileid.length()) {
    String text =  F("File uploaded to Drive with id ");
    text += fileid;
    server.send(200, "text/plain", text);
    Serial.println(text);
  }
  else {
    server.send(200, "text/plain", "Error");
    Serial.print("\nError. File not uploaded correctly.");
  }
  Serial.println("\n\nApp folder content:");
  myDrive.printFileList();
  vTaskDelete(NULL);
}



////////////////////////////////  Configure and start local webserver  /////////////////////////////////////////
void configureWebServer() {
  // Try to connect to flash stored SSID, start AP if fails after timeout
  IPAddress myIP = myWebServer.startWiFi(10000, "ESP_AP", "123456789" );

  // Configure /setup page and start Web Server
  myWebServer.addOption(FILESYSTEM, "Google Drive Folder Name", appFolderName);
  myWebServer.addOption(FILESYSTEM, "Local Folder Name", dataFolderName);

  // Add 2 buttons for ESP restart and ESP Google authorization page
  myWebServer.addOption(FILESYSTEM, "raw-html-button", button_html);

  String infoStr = String(info_html);
  infoStr.replace("SETUP_PAGE__PLACEHOLDER", hostname);
  Serial.println(infoStr);
  myWebServer.addOption(FILESYSTEM, "raw-html-info", infoStr);
  myWebServer.addHandler("/config", handleConfigPage);

  // Start webserver
  if (myWebServer.begin()) {
    Serial.print(F("ESP Web Server started on IP Address: "));
    Serial.println(myIP);
    Serial.println(F("Open /setup page to configure optional parameters"));
    Serial.println(F("Open /edit page to view and edit files"));
    MDNS.begin(hostname.c_str());
  }
}

/////////////////////////////////    SETUP        //////////////////////////////////
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);

  // FILESYSTEM init and load optins (ssid, password, etc etc)
  startFilesystem();
  loadApplicationConfig();
  Serial.println();

  // Set Google API certificate
#ifdef ESP8266
  client.setSession(&session);
  client.setTrustAnchors(&certificate);
  client.setBufferSizes(1024, 1024);
  WiFi.hostname(hostname.c_str());
  configTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
#elif defined(ESP32)
  client.setCACert(google_cert);
  WiFi.setHostname(hostname.c_str());
  configTzTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
#endif
  configureWebServer();
  Serial.print(F("Waiting Network Time server sync... "));
  getUpdatedtime(15000);
  Serial.println(F("done."));

  // Create Google Drive folder
  if (createDriveFolder()) {
    Serial.println("\n\nGoogle Drive folder present or created succesfull");
    authorized = true;
  }

}


/////////////////////////////////      LOOP        //////////////////////////////////
void loop() {

  // Run webserver (just for debug, not needed after first configuration)
  if (runWebServer || WiFi.status() != WL_CONNECTED ) {
    myWebServer.run();
#ifdef ESP8266
    MDNS.update();
#endif
  }

  // Add new measure once 5 seconds
  static uint32_t dataTime;
  if (millis() - dataTime > 5000) {
    dataTime = millis();
    appendMeasurement();
  }

  // Upload data file to Google Drive once a minute
  static uint32_t uploadTime;
  if (millis() - uploadTime > 60000 && authorized) {
    uploadTime = millis();
    // Start uploadToDrive in a parallel task
    xTaskCreate(
      uploadToDrive,    // Function to implement the task
      "uploadToDrive",  // Name of the task
      8192,             // Stack size in words
      NULL,             // Task input parameter
      1,                // Priority of the task
      NULL              // Task handle.
    );
  }

  // Blink built-in led to show NON-blocking working mode
  static uint32_t ledTime = millis();
  static bool doBlink = false;
  if (millis() - ledTime > 150) {
    ledTime = millis();
    doBlink = !doBlink;
    digitalWrite(LED_BUILTIN, doBlink);
  }
}
