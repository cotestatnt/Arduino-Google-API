#include <time.h>
#include <GoogleSheet.h>
#include <esp-fs-webserver.h>   // https://github.com/cotestatnt/esp-fs-webserver

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

// Timezone definition to get properly time from NTP server
#define MYTZ "CET-1CEST,M3.5.0,M10.5.0/3"
struct tm Time;

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
String hostname = HOSTNAME;
String folderName = FOLDER_NAME;
String spreadSheetName = S_FILENAME;
String sheetName = SHEETNAME;
unsigned long updateInterval = 30 * 1000;           // Add row to sheet every x seconds

const char* formulaExample = "\"=IF(ISNUMBER(B:B); B:B - C:C;)\"";

GoogleSheetAPI mySheet(FILESYSTEM, client );
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
      folderName = doc["Drive Folder Name"].as<String>();
      spreadSheetName = doc["Spreadsheet Filename"].as<String>();
      updateInterval = doc["Data Update Interval"];
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
    ESP.restart();
  }
}


////////////////////////////////  Create Google Sheet if necessary  /////////////////////////////////////////
bool createGoogleSheet() {
  if (WiFi.isConnected()) {
    // Begin Google Sheet handling (to store tokens and configuration, filesystem must be mounted before)
    if (mySheet.begin(client_id, client_secret, scopes, api_key, redirect_uri)) {

      // Authorization OK when we have a valid token
      if (mySheet.getState() == GoogleDriveAPI::GOT_TOKEN) {
        authorized = true;

        //Check if the "app folder" is present in your Drive and store the ID
        String folderId = mySheet.searchFile(folderName);
        mySheet.setAppFolderId(folderId);
        Serial.printf("App folder present, id: %s\n", folderId.c_str());

        // An ID founded for application folder (33 chars random string)
        if (folderId.length() >= 30 ) {

          // Create a new spreadsheet if not exist, return the sheet ID (44 random chars)
          String spreadSheetId = mySheet.searchFile(spreadSheetName, folderId);
          if (spreadSheetId.length() > 40) {
            mySheet.setSheetId(spreadSheetId);
            Serial.printf("Spreasheet is present, id: %s\n", spreadSheetId.c_str());
          }
          else {
            String newSheetId = mySheet.newSpreadsheet(spreadSheetName, sheetName, folderId);
            Serial.printf("New Spreasheet created, ID: %s\n", newSheetId.c_str());
            mySheet.setSheetId(newSheetId);
            delay(500);

            // Add header row  ['colA', 'colB', 'colC', .... ]
            String row = "['Timestamp', 'Max free heap', 'Max block size', 'Difference']";
            String range = sheetName;
            range += "!A1";
            mySheet.appendRowToSheet( mySheet.getSheetId(), range.c_str(),  row.c_str());
            return true;
          }
        }
        else {
          Serial.println(F("Folder APP not present. Now it will be created and then ESP will restart"));
          mySheet.createFolder(folderName.c_str(), "root");
          ESP.restart();
        }
      }
    }

    if (mySheet.getState() == GoogleOAuth2::INVALID) {
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
  myWebServer.addOption(FILESYSTEM, "Drive Folder Name", folderName);
  myWebServer.addOption(FILESYSTEM, "Spreadsheet Filename", spreadSheetName);
  myWebServer.addOption(FILESYSTEM, "Data Update Interval", updateInterval);

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


////////////////////////////////  Append new row of data to sheet  /////////////////////////////////////////
void appendData() {
  static uint32_t timeUpdate = 0;

  if (WiFi.status() == WL_CONNECTED && authorized) {
    if (millis() - timeUpdate > updateInterval) {
      // Append a new row to the sheet
      timeUpdate = millis();
#ifdef ESP32
      size_t free; size_t max;
      free = heap_caps_get_free_size(0);
      max = heap_caps_get_largest_free_block(0);
#elif defined(ESP8266)
      uint32_t free; uint16_t max;
      ESP.getHeapStats(&free, &max, nullptr);
#endif
      time_t rawtime = time(nullptr);
      Time = *localtime (&rawtime);
      char buffer [30];
      strftime (buffer, sizeof(buffer), "\"%d/%m/%Y %H:%M:%S\"", &Time);
      char row[200];
      snprintf(row, sizeof(row), "[%s, %d, %d, %s]", buffer, free, max, formulaExample);
      String range = sheetName;
      range += "!A1";
      if ( mySheet.appendRowToSheet( mySheet.getSheetId(), range.c_str(), row)) 
        Serial.printf("New row appended to %s::%s succesfully\n", spreadSheetName.c_str(), sheetName.c_str());
      else
        Serial.println("Failed to append data to Sheet");
    }
  }
  else {
    static uint32_t reconTime;
    if (millis() - reconTime > 5000) {
      WiFi.begin();
      Serial.print("*-");
      reconTime = millis();
    }
  }
}

void setup() {
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

  // Create Google sheet
  if (createGoogleSheet()) {
    Serial.println("Google Sheet created succesfull");
  }

  // Sync with NTP timeserver
  getUpdatedtime(5000);
  char _time [30];
  strftime (_time, sizeof(_time), "%d/%m/%Y %H:%M:%S", &Time);
  Serial.println(_time);
}


void loop() {
  if (runWebServer || WiFi.status() != WL_CONNECTED ) {
    myWebServer.run();
#ifdef ESP8266
    MDNS.update();
#endif
  }

  // Append new data to Google Sheet
  appendData();
}
