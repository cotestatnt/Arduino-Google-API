#include <time.h>
#include <GoogleOAuth2.h>
#include <esp-fs-webserver.h>   // https://github.com/cotestatnt/esp-fs-webserver

// Timezone definition to obtain the correct time from the NTP server
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
  Session   session;
  X509List  certificate(google_cert);
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

/* The instance of library that will handle authorization token renew */
GoogleOAuth2 myAuth(FILESYSTEM, client);
const char* hostname = HOSTNAME;
bool authorized = false;


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


////////////////////////////////  Configure and start local webserver  /////////////////////////////////////////

/* This is the webpage used to authorize the application (OAuth2.0) */
#include "gaconfig_htm.h"

/* Handle /config webpage request */
void handleConfigPage() {
  WebServerClass* webRequest = myWebServer.getRequest();
  webRequest->sendHeader(PSTR("Content-Encoding"), "gzip");
  webRequest->send_P(200, "text/html", AUTHPAGE_HTML, AUTHPAGE_HTML_SIZE);
}

/* Start local webserver */
void configureWebServer() {
  // Try to connect to flash stored SSID, start AP if fails after timeout
  IPAddress myIP = myWebServer.startWiFi(10000, "ESP_AP", "123456789" );

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
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void setup() {
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
 * Configure the local web server using the esp-fs-webserver library.
 * This will also handle WiFi connection, the Captive Portal, and the included WiFiManager.
 * The web server can be extended with custom options or web pages for other purposes.
 * Check the examples included with the library at: https://github.com/cotestatnt/esp-fs-webserver
 */
  configureWebServer();

  /* Check if application is authorized for selected scope */
  if (myAuth.begin(client_id, client_secret, scopes, api_key, redirect_uri)) {
    if (myAuth.getState() == GoogleOAuth2::GOT_TOKEN) {
      /* Application authorized, nothing else to do */
      Serial.print(success_message);
    }
    else {
      /* Application not authorized, check your credentials in config.h and perform again authorization procedure */
      Serial.print(warning_message);
    }
  }

}

void loop() {

  if (!myAuth.isAuthorized()) {
    /*
    * If not authorized run webserver to handle /config webpage requests.
    * For debug purpose, you would leave the webserver run also after authorization successfully
    */
    myWebServer.run();
#ifdef ESP8266
    MDNS.update();
#endif
  }
}