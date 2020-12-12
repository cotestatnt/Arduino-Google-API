/*
 Name:	      GoogleEmail.ino
 Created:     26/11/2020
 Author:      Tolentino Cotesta <cotestatnt@yahoo.com>
 Description: this example show how to read and send email using Gmail API. Work both with ESP8666 and ESP32.
 The sendEmail() function is called from the /email.htm web page, but of course it can be called with any condition.
 
 This example include a local webserver for easy management of files and folder (it's just customized version of
 FSBrowser example included with Arduino core for ESP8266)
 The edit page http://espfs.local/edit can be optionally stored in flash via PROGMEM keyword (espWebServer.h), 
 so you can easily upload or edit your files only with a web browser (only 6Kb of flash).
 ESP32 platform could have a little bug to fix https://github.com/espressif/arduino-esp32/issues/3652
*/

#include <FS.h>
#include <time.h>
#include <sys/time.h>             
#include <GoogleGmail.h>

// Timezone definition to get properly time from NTP server
#define MYTZ "CET-1CEST,M3.5.0,M10.5.0/3"

#ifdef ESP8266
  // With ESP8266 is better to use LittleFS
  #include <LittleFS.h>
  const char* fsName = "LittleFS"; 
  #define fileSystem LittleFS
  LittleFSConfig fileSystemConfig = LittleFSConfig();  
#elif defined(ESP32) 
  // LittleFS is not supported on ESP32 platform, use FFat or SPIFFS
  #include <FFat.h>
  const char* fsName = "FFat"; 
  #define fileSystem FFat
#endif

static tm timeinfo;
const char * getNTPtime(const uint32_t timeout) ;

bool fsOK = false;
const char *hostname = "espfs";
#include "espWebserver.h"

#define FORMAT_IF_FAILED true   // Format filesystem if needed

// WiFi setup
const char* ssid        =  "ssid";     // SSID WiFi network
const char* password    =  "xxxx";     // Password  WiFi network   

GmailList myEmailList;
GoogleGmailAPI myGmail(&fileSystem, "/gmail.json", &myEmailList);

void setup()
{
    Serial.begin(115200);

    // WiFi INIT
    startWiFi();

    // FILESYSTEM INIT
    startFilesystem();

    // Scopes with "TV and limited input devices" don't include Gmail functionality, so here we assume that a valid
    // configuration files created with "Web Server Application" flow was present in the ESP fylesystem.
    if( myGmail.begin()){
        
		// myGmail.getMailList("cotestatnt@yahoo.com", true, 10);  // Get last 10 unread messages sent from provided address
		myGmail.getMailList(nullptr, true, 10);      // Get last 10 unread messages
		Serial.println(F("Unreaded email: "));
		myEmailList.printList();

		// Get metadata of the first unread emssage
		if(myEmailList.size() > 0){
			const char * id = myEmailList.getMailId(0);
			myGmail.getMailData(id);
			
			Serial.print(F("\nFrom:   "));
			Serial.println(myEmailList.getFrom(id));
			Serial.print(F("Subject:  "));
			Serial.print(myEmailList.getSubject(id));       
			Serial.print(F(" ("));
			Serial.print(myEmailList.getDate(id));
			Serial.println(F(")"));
			
			// Read complete message text
			//Serial.println(myGmail.readMail(id));

			// Or you can read only snippet text
			Serial.println(F("\nMail snippet:"));
			Serial.println(myGmail.readMail(id, true));
			Serial.println(F("--------------End of email-------------"));
		}
	}

    if(myGmail.getState() == GoogleOAuth2::INVALID){
        Serial.println(F("\n\nGoogle says that your client is NOT VALID!\nCheck configuration file content.\n")); 
    }
        
   
    // Sync time with NTP. Blocking, but with timeout (0 == no timeout)
    const char* nowTime = getNTPtime(5000);    
    Serial.print("Time: "); 
    Serial.println(nowTime); 

    // Add custom handler for webserver
    server.on("/sendEmail", HTTP_GET, sendEmail);
    // WEB SERVER INIT
    startWebServer(); 
}


void loop()
{
    server.handleClient();
#ifndef ESP32
    MDNS.update();
#endif
}


void sendEmail()
{
    String toAddr = server.arg("to"); 
    String subject = server.arg("subject"); 
    String body = server.arg("body"); 
    
    if(server.arg("signature").toInt()){
        const char* nowTime = getNTPtime(0); 
        String signature = F("\r\n\r\nMessage sent from http://");
        signature += hostname;
        signature += F(".local/ - ");
        signature += nowTime;
        body += signature;
    }

    // We have collected all necessary data from browser, now send the email
    if(toAddr.length() && subject.length() && body.length()){
        const char* mailId = myGmail.sendEmail(toAddr, subject, body);
        if (strlen(mailId) > 0)
            Serial.print("New email sent to: ");   Serial.print(toAddr);
        // Send back Gmail id to browser as response
        server.send(200, "text/plain", mailId);
    }
    else {
        // Send back error message to browser as response
        server.send(400, "Data required", "Error: missing required data");
    }
}


void startFilesystem()
{
  // FILESYSTEM INIT
#ifdef ESP8266
    fileSystemConfig.setAutoFormat(true);
    fileSystem.setConfig(fileSystemConfig);
    fsOK = fileSystem.begin();
  
#elif defined(ESP32) 
    //fileSystem.format();
    fsOK = fileSystem.begin(FORMAT_IF_FAILED);
#endif

    if (fsOK){
        File root = fileSystem.open("/", "r");
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


void startWiFi()
{
#ifdef ESP8266
    WiFi.hostname(hostname);  
#elif defined(ESP32) 
    WiFi.setHostname(hostname);
#endif 
    Serial.printf("Connecting to %s\n", ssid);
    if (String(WiFi.SSID()) != String(ssid)){
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, password);
    }
    while (WiFi.status() != WL_CONNECTED){
        delay(500);
        Serial.print(".");
    }
    Serial.print("\nConnected! IP address: ");
    Serial.println(WiFi.localIP());

    MDNS.begin(hostname);
    Serial.printf("Open http://%s.local/edit to edit or upload files\n", hostname);    

    // Set timezone and NTP servers
#ifdef ESP8266
    configTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
#elif defined(ESP32) 
    configTzTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
#endif
}


const char * getNTPtime(const uint32_t timeout) 
{
#define BUF_SIZE 40
    uint32_t start = millis();
    Serial.print("Sync time with NTP server...");
    do {
        time_t now = time(nullptr); 
        timeinfo = *localtime(&now);
    } while(millis() - start < timeout  && timeinfo.tm_year <= (1970 - 1900));

    Serial.println(" done."); 

    char * buffer = new char[BUF_SIZE];
    buffer[BUF_SIZE] = '\0';

    strftime (buffer, 40, "%A, %d/%m/%Y %H:%M:%S", &timeinfo);
    return (const char*) buffer;
}