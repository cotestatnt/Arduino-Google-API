#include <time.h>
#include <GoogleCalendar.h>
#include <esp-fs-webserver.h>   // https://github.com/cotestatnt/esp-fs-webserver

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

/* The istance of library that will handle authorization token renew */
GoogleOAuth2 myAuth(FILESYSTEM, client);
EventList eventList;
GoogleCalendarAPI calendar(myAuth, eventList);

const char* hostname = "esp2calendar";
String calendarId;

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

/* This is the webpage used for authorize the application (OAuth2.0) */
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
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void setup() {
  Serial.begin(115200);
  Serial.println();

  /* FILESYSTEM init and print list of file*/
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
    /* Application authorized, nothing else to do */
    Serial.print(success_message);

    /* Check if a calendar is present or create (default calendar ID is equal to account gmail address) */
    calendarId = calendar.checkCalendar("ESP Test Calendar", true, "Europe/Rome");
    Serial.printf("Calendar id: %s\n", calendarId.c_str());

    /* get event list starting from now */
    calendar.getEventList(calendarId.c_str());
    calendar.prinEventList();

    /* Create a new event using a simple text sentence */
    //String eventId = calendar.quickAddEvent(calendarId.c_str(), "Simple Test Event 10/06/2022 15:00 18:00");
    //Serial.printf("New event added, id: %s\n", eventId.c_str());

    //delay(5000);
    //calendar.deleteEvent(calendarId, eventId);

    /*
    * Create a new event passing all parameters.
    * You can create a time_t variable e.g. with class utility "getEpochTime()"
    * The default RRULE is for a single event with no recurrency (you can omit last 2 params)
    */
    time_t start = calendar.getEpochTime(2022, 06, 14, 17, 0);
    time_t end = calendar.getEpochTime(2022, 06, 14, 20, 0);
    String eventId = calendar.addEvent(calendarId.c_str(), "Test event", start, end, GoogleCalendarAPI::DAILY, 3);

    /* As alternative you can use directly a RRULE sentence or a list of rules. Ex: "RRULE:FREQ=DAILY;COUNT=3" */
    // String eventId = calendar.addEvent(calendarId.c_str(), "Test event", start, end, "RRULE:FREQ=DAILY;COUNT=3");
    Serial.printf("New event added, id: %s\n", eventId.c_str());
  }
  else {
    /* Application not authorized, check your credentials in config.h and perform again authorization procedure */
    Serial.print(warning_message);
  }

}

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

  /* check if an event occurs in the next hour */
  static uint32_t checkTime;
  if (millis() -checkTime > 30000) {
    checkTime = millis();
    time_t now = time(nullptr);
    int evCount = calendar.getEventList(calendarId.c_str(), now, now + 3600);
    for (int i=0; i < evCount; i++) {

      if (calendar.eventInProgress(i)) {
        EventList::EventItem* event = calendar.getEvent(i);
        Serial.printf("The event \"%s\" is actually in progress\n", event->summary.c_str());
      }

    }
  }
}
