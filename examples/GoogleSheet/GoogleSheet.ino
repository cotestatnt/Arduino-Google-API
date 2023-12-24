#include <time.h>
#include <GoogleSheet.h>
#include <esp-fs-webserver.h> // https://github.com/cotestatnt/esp-fs-webserver

// Timezone definition to get properly time from NTP server
#define MYTZ "CET-1CEST,M3.5.0,M10.5.0/3"
struct tm Time;

// Filesystem will be used to store access credentials and custom options
#include <FS.h>
#include <LittleFS.h>
#define FILESYSTEM LittleFS

#include <WiFiClientSecure.h>
#ifdef ESP8266
#include <ESP8266WiFi.h>
using WebServerClass = ESP8266WebServer;
Session session;
X509List certificate(google_cert);
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

/* The istance of library that will handle Sheet API.
 * GoogleFilelist object is optional, but can take a local reference to remote IDs
 * in order to speed up file operations like searching or similar.
 */
GoogleSheetAPI mySheet(&myAuth);

const char *hostname = "esp2sheet";

String spreadSheetId;
String folderName = "SheetFolder";
String spreadSheetName = "mySheet";
String sheetName = "myData";
unsigned long updateInterval = 30 * 1000; // Add row to sheet every x seconds

const char *formulaExample = "\"=IF(ISNUMBER(B:B); B:B - C:C;)\"";

////////////////////////////////  NTP Time  /////////////////////////////////////////
void getUpdatedtime(const uint32_t timeout)
{
  uint32_t start = millis();
  do
  {
    time_t now = time(nullptr);
    Time = *localtime(&now);
    delay(1);
  } while (millis() - start < timeout && Time.tm_year <= (1970 - 1900));
}

////////////////////////////////  Start Filesystem  /////////////////////////////////////////
void startFilesystem()
{
  if (!LittleFS.begin())
  {
    Serial.println("LittleFS Mount Failed");
    return;
  }
  else
  {
    File root = FILESYSTEM.open("/", "r");
    File file = root.openNextFile();
    while (file)
    {
      const char *fileName = file.name();
      size_t fileSize = file.size();
      Serial.printf("FS File: %s, size: %lu\n", fileName, (long unsigned)fileSize);
      file = root.openNextFile();
    }
    Serial.println();
  }
}

////////////////////////////////  Configure and start local webserver  /////////////////////////////////////////

/* This is the webpage used for authorize the application (OAuth2.0) */
#include "gaconfig_htm.h"

/* Handle /config webpage request */
void handleConfigPage()
{
  WebServerClass *webRequest = myWebServer.getRequest();
  webRequest->sendHeader(PSTR("Content-Encoding"), "gzip");
  webRequest->send_P(200, "text/html", AUTHPAGE_HTML, AUTHPAGE_HTML_SIZE);
}

/* Start local webserver */
void configureWebServer()
{
  // Try to connect to flash stored SSID, start AP if fails after timeout
  IPAddress myIP = myWebServer.startWiFi(10000, "ESP_AP", "123456789");

  // Add 2 buttons for ESP restart and ESP Google authorization page
  myWebServer.addOption(FILESYSTEM, "raw-html-button", button_html);

  String infoStr = String(info_html);
  infoStr.replace("SETUP_PAGE__PLACEHOLDER", hostname);
  myWebServer.addOption(FILESYSTEM, "raw-html-info", infoStr);
  myWebServer.addHandler("/config", handleConfigPage);

  // Start webserver
  if (myWebServer.begin())
  {
    Serial.print(F("ESP Web Server started on IP Address: "));
    Serial.println(myIP);
    Serial.println(F("Open /setup page to configure optional parameters"));
    Serial.println(F("Open /edit page to view and edit files"));
    MDNS.begin(hostname);
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////  Append new row of data to sheet  /////////////////////////////////////////
void appendData()
{
  static uint32_t timeUpdate = 0;

  if (millis() - timeUpdate > updateInterval)
  {
    // Append a new row to the sheet
    timeUpdate = millis();
#ifdef ESP32
    size_t free;
    size_t max;
    free = heap_caps_get_free_size(0);
    max = heap_caps_get_largest_free_block(0);
#elif defined(ESP8266)
    uint32_t free;
    uint16_t max;
    ESP.getHeapStats(&free, &max, nullptr);
#endif
    time_t rawtime = time(nullptr);
    Time = *localtime(&rawtime);
    char buffer[30];
    strftime(buffer, sizeof(buffer), "\"%d/%m/%Y %H:%M:%S\"", &Time);
    char row[200];
    snprintf(row, sizeof(row), "[%s, %d, %d, %s]", buffer, free, max, formulaExample);
    String range = sheetName;
    range += "!A1";
    if (mySheet.appendRowToSheet(spreadSheetId.c_str(), range.c_str(), row))
      Serial.printf("New row appended to %s::%s succesfully\n", spreadSheetName.c_str(), sheetName.c_str());
    else
      Serial.println("Failed to append data to Sheet");
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println();

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

  /* Get updated local time from NTP */
  getUpdatedtime(10000);

  /* FILESYSTEM init and print list of file*/
  startFilesystem();

  /*
   * Configure local web server: with esp-fs-webserver library,
   * this will handle also the WiFi connection, the Captive Portal and included WiFiManager.
   * The webserver can be also extended with custom options or webpages for other purposes.
   * Check the examples included with library https://github.com/cotestatnt/esp-fs-webserver
   */
  configureWebServer();

  /* Check if application is authorized for selected scope */
  if (myAuth.begin(client_id, client_secret, scopes, api_key, redirect_uri))
  {
    if (myAuth.getState() == GoogleOAuth2::GOT_TOKEN)
    {
      /* Application authorized */
      Serial.print(success_message);

      // Check if the spreadsheet is already created and inside provided folder (create if not)
      spreadSheetId = mySheet.isSpreadSheet(spreadSheetName.c_str(), folderName.c_str(), true);
      if (spreadSheetId.length() >= MIN_ID_LEN)
      {
        // A valid Google spreadsheet id founded
        Serial.printf("Spreasheet is present, id: %s\n", spreadSheetId.c_str());
      }
      else
      {
        // Create a new spreadsheet if not exist, return the sheet ID (44 random chars)
        spreadSheetId = mySheet.newSpreadsheet(spreadSheetName.c_str(), mySheet.getParentId());
        Serial.printf("New Spreasheet created, id: %s\n", spreadSheetId.c_str());
        delay(500);

        /////////   Optional: rename default sheet (id == 0) with provided name  ////////////
        if (mySheet.setSheetTitle(sheetName.c_str(), spreadSheetId.c_str()))
        {
          Serial.printf("Sheet title updated to \"%s\" (%s)\n", sheetName.c_str(), spreadSheetId.c_str());
        }
        /////////////////////////////////////////////////////////////////////////////////////

        /////////   Optional: create an additional sheet with provided name  ////////////
        /*
        uint32_t newSheetId = mySheet.newSheet("testSheet", spreadSheetId.c_str());
        if (newSheetId)
          Serial.printf("New sheet \"testSheet\" added to spreadsheet, id: %ld\n", newSheetId);
        */
        /////////////////////////////////////////////////////////////////////////////////////

        // Add first header row  ['colA', 'colB', 'colC', .... ]
        String row = "['Timestamp', 'Max free heap', 'Max block size', 'Difference']";
        String range;
        // range += sheetName;
        range += "!A1";
        mySheet.appendRowToSheet(spreadSheetId, range, row);
      }
    }
    else
    {
      /* Application not authorized, check your credentials in config.h and perform again authorization procedure */
      Serial.print(warning_message);
    }
  }
}

void loop()
{

  if (!myAuth.isAuthorized())
  {
#ifdef ESP8266
    MDNS.update();
#endif
    /*
     * If not authorized run webserver to handle /config webpage requests.
     * For debug purpose, you would leave the webserver run also after authorization successfully
     */
    myWebServer.run();
  }
  else
  {
    // Append new data to Google Sheet
    static uint32_t appendTime;
    if (millis() - appendTime > 30000)
    {
      appendTime = millis();
      appendData();
    }
  }
}
