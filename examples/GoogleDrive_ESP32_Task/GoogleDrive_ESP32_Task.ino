#include <time.h>
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

// WiFi setup
String hostname = HOSTNAME;
// Remote
String appFolderName = FOLDER_NAME;     // Google Drive online folder name
String dataFileName = S_FILENAME;       // Google Drive filename
// Local (data filename will be date-related once for day  ex. "20201025.txt")
#define MAX_NAME_LEN 16
String dataFolderName = FOLDER_NAME;    // Local folder for store data files
char dataFilePath[strlen(FOLDER_NAME) + MAX_NAME_LEN +1];

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
  File file = FILESYSTEM.open("/config.json", "r");
  if (file) {
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) {
      Serial.println(F("Failed to deserialize file, may be corrupted"));
      Serial.println(error.c_str());
    }
    else {
      serializeJsonPretty(doc, Serial);
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

////////////////////////////////  Heap memory info  /////////////////////////////////
void printHeapStats() {
  static uint32_t heapTime;
  if (millis() - heapTime > 10000) {
    heapTime = millis();
    time_t now = time(nullptr);
    Time = *localtime(&now);
#ifdef ESP32
    Serial.printf("%02d:%02d:%02d - Total free: %6d - Max block: %6d\n", Time.tm_hour, Time.tm_min, Time.tm_sec, heap_caps_get_free_size(0), heap_caps_get_largest_free_block(0) );
#elif defined(ESP8266)
    uint32_t free;
    uint16_t max;
    ESP.getHeapStats(&free, &max, nullptr);
    Serial.printf("%02d:%02d:%02d - Total free: %5d - Max block: %5d\n", Time.tm_hour, Time.tm_min, Time.tm_sec, free, max);
#endif
  }
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
  if (Time.tm_year <= (1970 - 1900)) {
    Serial.println("NTP time is not synced");
    return;
  }
  // Create name of the data file (if it's a new day, will be created a new data file)
  char dataFileName[MAX_NAME_LEN + 1];          // ex. "20201025.txt"  
  snprintf(dataFileName, MAX_NAME_LEN, "%04d%02d%02d.txt", Time.tm_year + 1900, Time.tm_mon + 1, Time.tm_mday );
  snprintf(dataFilePath, 30, "/%s/%s", dataFolderName, dataFileName);
  
  getUpdatedtime(100);
  uint32_t free; uint16_t max;
#ifdef ESP32
  free = heap_caps_get_free_size(0);
  max = heap_caps_get_largest_free_block(0);
#elif defined(ESP8266)
  ESP.getHeapStats(&free, &max, nullptr);
#endif
  
  if (FILESYSTEM.exists(dataFilePath)) {
    // Add updated data to file
    char dataBuf[30];
    snprintf(dataBuf, sizeof(dataBuf), "%02d:%02d:%02d; %5d; %5d", Time.tm_hour, Time.tm_min, Time.tm_sec, free, max);
    File file = FILESYSTEM.open(dataFilePath, FILE_APPEND);
    file.println(dataBuf);
    file.close();
    Serial.print(F("Appended new row to "));
    Serial.print(dataFilePath);
    Serial.print(F(": "));
    Serial.println(dataBuf);
  }
  else {
    Serial.print(F("Create new file "));
    Serial.println(dataFilePath);
    createDayFile(dataFilePath);
  }
}

////////////////////////////////  Upload local data file to Google Drive  /////////////////////////////////////////
/*
    Uploading big file to Google Drive can block the MCU for several seconds.
    Let's use a FreeRTOS task to handle it keeping the MCU free to do whatever else meanwhile
*/
void uploadToDrive(void * args) {
  uint32_t startUpload = millis();

  // Check if file is already in file list (local)
  String fileid = myDrive.getFileId(dataFileName.c_str());

  // Check if a file with same name is already present online and return id
  // (Google assume that file is different also if name is equal)
  if (fileid.length() < 30)
    fileid = myDrive.searchFile(dataFileName);

  if (fileid.length() > 30) {
    Serial.print(fileid);
    Serial.print(" - file present in app folder. \nCall update method for ");
    Serial.println(dataFilePath);
    fileid = myDrive.uploadFile(dataFilePath, fileid.c_str(), true);
  }
  else {
    Serial.print("\n\nFile not present in app folder. Call upload method for");
    Serial.println(dataFilePath);
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

////////////////////////////////  Start Filesystem  /////////////////////////////////////////
void startFilesystem() {
  if ( FILESYSTEM.begin()) {
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
  else {
    Serial.println("ERROR on mounting filesystem. It will be formmatted!");
    FILESYSTEM.format();
    delay(5000);
    ESP.restart();
  }
}

////////////////////////////////  Create Google Drive folder  /////////////////////////////////////////
bool createFolder() {
  if (WiFi.isConnected()) {
    // Begin Google API handling (to store tokens and configuration, filesystem must be mounted before)
    if (myDrive.begin(client_id, client_secret, scopes, api_key, redirect_uri)) {
      // Authorization OK when we have a valid token
      if (myDrive.getState() == GoogleOAuth2::GOT_TOKEN) {
        Serial.print(F("\n\n-------------------------------------------------------------------------------"));
        Serial.print(F("\nYour application has the credentials to use the google API in the selected scope\n"));
        Serial.print(F("\n---------------------------------------------------------------------------------\n\n"));
        String appFolderId =  myDrive.searchFile(appFolderName);
        // Save the folder id for easy retrieve when needed
        myDrive.setAppFolderId(appFolderId);

        Serial.print("App folder id: ");
        Serial.println(appFolderId);
        if (appFolderId.length() >= 30 ) {
          // A valid Google id founded
          Serial.println("\n\nFile created with this app:");
          myDrive.updateFileList();
          myDrive.printFileList();
        }
        else {
          Serial.println("Folder APP not present. Now it will be created.");
          myDrive.createFolder(appFolderName.c_str(), "root");
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
  if (createFolder()) {
    Serial.println("Google Drive folder created succesfull");
  }

  // Append to the file the first measure on reboot
  appendMeasurement();
}

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
