
#include <time.h>
#include <GoogleDrive.h>
#include <esp-fs-webserver.h>   // https://github.com/cotestatnt/esp-fs-webserver

// Timezone definition to get properly time from NTP server
#define MYTZ "CET-1CEST,M3.5.0,M10.5.0/3"
struct tm Time;

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

// Filesystem will be used to store access credentials and custom options
#include <FS.h>
#include <LittleFS.h>
#define FILESYSTEM LittleFS

#include <WiFiClientSecure.h>
#ifdef ESP8266
#include <ESP8266WiFi.h>
  using WebServerClass = ESP8266WebServer;
  Session   session;
  X509List  certificate(google_cert);
  #define FILE_READ "r"
  #define FILE_WRITE "w"
  #define FILE_APPEND "a"
#elif defined(ESP32)
  #include <WiFi.h>
  using WebServerClass = WebServer;
#endif

#include "config.h"

/* The webserver it's needed only the first time in order to do OAuth2.0 authorizing workflow */
WebServerClass server;
FSWebServer myWebServer(FILESYSTEM, server);

/* The web client used from library */
WiFiClientSecure client;

/* The istance of library that will handle authorization token renew */
GoogleOAuth2 myAuth(FILESYSTEM, client);

/* The istance of library that will handle Drive API.
* GoogleFilelist object is optional, but can take a local reference to remote IDs
* in order to speed up file operations like searching or similar.
*/
GoogleFilelist driveList;
GoogleDriveAPI myDrive(&myAuth, &driveList);

const char* hostname = "esp2drive";
#define APP_FOLDER  "myDriveFolder" // Google Drive online folder name
#define DATA_FOLDER "datalog"       // Local filesystem folder for data

/* Local data filename will be date-related (once for day ex. "\datalog\20201025.txt") */
#define MAX_NAME_LEN 16
char dataFilePath[strlen(DATA_FOLDER) + MAX_NAME_LEN + 3];
char dataFileName[MAX_NAME_LEN + 1];    // ex. "20201025.txt"

// This is the webpage used for authorize the application (OAuth2.0)
#include "gaconfig_htm.h"
void handleConfigPage() {
  WebServerClass* webRequest = myWebServer.getRequest();
  webRequest->sendHeader(PSTR("Content-Encoding"), "gzip");
  webRequest->send_P(200, "text/html", AUTHPAGE_HTML, AUTHPAGE_HTML_SIZE);
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

  /* Get updated local time from NTP */
  getLocalTime(&Time);
  
  uint32_t free, max;
#ifdef ESP32
  free = heap_caps_get_free_size(0);
  max = heap_caps_get_largest_free_block(0);
#elif defined(ESP8266)
  ESP.getHeapStats(&free, &max, nullptr);
#endif

  // Create name of the data file (if it's a new day, will be created a new data file)  
  snprintf(dataFileName, MAX_NAME_LEN, "%04d%02d%02d.txt", Time.tm_year + 1900, Time.tm_mon + 1, Time.tm_mday );
  snprintf(dataFilePath, 30, "/%s/%s", DATA_FOLDER, dataFileName);
  
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
  if (!LittleFS.begin()) {
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
  IPAddress myIP = myWebServer.startWiFi(20000, "ESP_AP", "123456789" );

  // Add 2 buttons for ESP restart and ESP Google authorization page
  myWebServer.addOption(FILESYSTEM, "raw-html-button", button_html);

  String infoStr = String(info_html);
  infoStr.replace("SETUP_PAGE__PLACEHOLDER", hostname);
  myWebServer.addOption(FILESYSTEM, "raw-html-info", infoStr);
  myWebServer.addHandler("/config", handleConfigPage);

  // Start webserver
  if (myWebServer.begin()) {
    Serial.print(F("ESP Web Server started on IP Address: "));
    Serial.println(myIP);
    Serial.println(F("Open /setup page to configure optional parameters"));
    Serial.println(F("Open /edit page to view and edit files"));
    MDNS.begin(hostname);
  }
}


/////////////////////////////////    SETUP        //////////////////////////////////
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  Serial.println();
  
  // FILESYSTEM init and load optins (ssid, password, etc etc)
  startFilesystem();

  /* Set Google API web server SSL certificate and NTP timezone */
#ifdef ESP8266
  client.setSession(&session);
  client.setTrustAnchors(&certificate);
  client.setBufferSizes(1024, 1024);
  WiFi.hostname(hostname);
  configTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
#elif defined(ESP32)
  client.setCACert(google_cert);
  WiFi.setHostname(hostname);
  configTzTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
#endif

  /*
  * Configure local web server: with esp-fs-webserver library,
  * this will handle also the WiFi connection, the Captive Portal and included WiFiManager.
  * The webserver can be also extended with custom options or webpages for other purposes.
  * Check the examples included with library https://github.com/cotestatnt/esp-fs-webserver
  */
  configureWebServer();

  /* Get updated local time from NTP */
  getLocalTime(&Time, 10000);
  char str[32];
  strftime(str, sizeof(str), "%Y-%m-%dT%H:%M:%S%z", &Time);
  Serial.println(str);

  /* Check if application is authorized for selected scope */
  if (myAuth.begin(client_id, client_secret, scopes, api_key, redirect_uri)) {
    if (myAuth.getState() == GoogleOAuth2::GOT_TOKEN) {
      /* Application authorized, nothing else to do */
      Serial.print(success_message);
      String appFolderId =  myDrive.searchFile(APP_FOLDER);

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
        Serial.printf("Folder %s not present. Now it will be created.\n", APP_FOLDER);
        appFolderId = myDrive.createFolder(APP_FOLDER, "root");
        myDrive.setAppFolderId(appFolderId);
        Serial.printf("App folder id: %s\n", appFolderId.c_str());
      }
    }
    else {
      /* Application not authorized, check your credentials in config.h and perform again authorization procedure */
      Serial.print(warning_message);
    }
  }
}


/////////////////////////////////      LOOP        //////////////////////////////////
void loop() {
  
  if (/*!myAuth.isAuthorized()*/true) {
    /*
    * If not authorized run webserver to handle /config webpage requests.
    * For debug purpose, you would leave the webserver run also after authorization successfully
    */
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
  if (millis() - uploadTime > 60000 && myAuth.isAuthorized()) {
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