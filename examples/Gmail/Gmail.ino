#include <time.h>
#include <GoogleGmail.h>
#include <esp-fs-webserver.h> // https://github.com/cotestatnt/esp-fs-webserver

// Timezone definition to get properly time from NTP server
#define MYTZ "CET-1CEST,M3.5.0,M10.5.0/3"
struct tm Time;

///////////////////////////////////////////////////////////////////////////
// Add this scopes to your project in order to be able to handle Gmail API
// https://mail.google.com/ https://www.googleapis.com/auth/gmail.modify
///////////////////////////////////////////////////////////////////////////

#include <FS.h>
#include <LittleFS.h>
#define FILESYSTEM LittleFS

#ifdef ESP8266
ESP8266WebServer server(80);
BearSSL::WiFiClientSecure client;
BearSSL::Session session;
BearSSL::X509List certificate(google_cert);
#elif defined(ESP32)
WiFiClientSecure client;
WebServer server;
#endif

#include "config.h"

GmailList mailList;
GoogleGmailAPI email(FILESYSTEM, client, mailList);
FSWebServer myWebServer(FILESYSTEM, server);
bool runWebServer = true;
bool authorized = false;

const char* hostname = "espGmailer";

// This is the webpage used for authorize the application (OAuth2.0)
#include "gaconfig_htm.h"
void handleConfigPage()
{
  WebServerClass *webRequest = myWebServer.getRequest();
  webRequest->sendHeader(PSTR("Content-Encoding"), "gzip");
  webRequest->send_P(200, "text/html", AUTHPAGE_HTML, AUTHPAGE_HTML_SIZE);
}

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
  if (FILESYSTEM.begin())
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
  else
  {
    Serial.println("ERROR on mounting filesystem. It will be formmatted!");
    FILESYSTEM.format();
    ESP.restart();
  }
}

////////////////////////////////  Chech GMail API configuration  /////////////////////////////////////////
bool configEmailer()
{
  if (WiFi.isConnected())
  {
    // Begin Google API handling (to store tokens and configuration, filesystem must be mounted before)
    if (email.begin(client_id, client_secret, scopes, api_key, redirect_uri))
    {
      // Authorization OK when we have a valid token
      if (email.getState() == GoogleOAuth2::GOT_TOKEN)
      {
        Serial.print(F("\n\n-------------------------------------------------------------------------------"));
        Serial.print(F("\nYour application has the credentials to use the google API in the selected scope\n"));
        Serial.print(F("\n---------------------------------------------------------------------------------\n"));
        authorized = true;

        // myGmail.getMailList("cotestatnt@yahoo.com", true, 10);  // Get last 10 unread messages sent from provided address
        email.getMailList(nullptr, true, 10);      // Get last 10 unread messages
        Serial.println(F("Unreaded email: "));
        mailList.printList();

        if (mailList.size() > 0) {
          // Get the id of the last received unread message (that match the filter of getMailList())
          const char * id = mailList.getMailId(0);

          // Read only snippet text
          // String mail_body = email.readMail(id, true);
          // Read complete message text
          String mail_body = email.readMail(id);

          Serial.print(F("\nFrom:   "));
          Serial.println(mailList.getFrom(id));
          Serial.print(F("Subject:  "));
          Serial.println(mailList.getSubject(id));
          Serial.print(F("Date: "));
          Serial.println(mailList.getDate(id));
          Serial.println(F("Body:"));
          Serial.println(mail_body);
          Serial.println(F("--------------End of email-------------"));
          email.setMessageRead(id);
        }

        //////////////////////////////// Send Test email ///////////////////////////////////////////
        email.sendEmail("cotestatnt@yahoo.com", "Ciao Tolentino!", "Hello world from Google API Client");
      }
    }

    if (email.getState() == GoogleOAuth2::INVALID)
    {
      Serial.print(warning_message);
      runWebServer = true;
      return false;
    }
  }
  return false;
}

////////////////////////////////  Configure and start local webserver  /////////////////////////////////////////
void configureWebServer()
{
  // Try to connect to flash stored SSID, start AP if fails after timeout
  IPAddress myIP = myWebServer.startWiFi(10000, hostname, "");

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


void setup()
{
  Serial.begin(115200);

  // FILESYSTEM init and load optins (ssid, password, etc etc)
  startFilesystem();
  Serial.println();

  // Set Google API certificate
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
  configureWebServer();

  // Create Google sheet
  if (configEmailer()) {
    Serial.println("Google Sheet created succesfull");
  }

  // Sync with NTP timeserver
  getUpdatedtime(5000);
  char _time[30];
  strftime(_time, sizeof(_time), "%d/%m/%Y %H:%M:%S", &Time);
  Serial.println(_time);

}

void loop()
{
  if (runWebServer || WiFi.status() != WL_CONNECTED)
  {
    myWebServer.run();
#ifdef ESP8266
    MDNS.update();
#endif
  }
}