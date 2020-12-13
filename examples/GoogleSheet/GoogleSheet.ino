/*
 Name:        GoogleSheet.ino
 Created:     12/12/2020
 Author:      Tolentino Cotesta <cotestatnt@yahoo.com>
 Description: this example show how to create and populate spreadsheet using Google Sheet API. Work with both ESP8666 and ESP32.
 
 This example include a local webserver for easy management of files and folder (it's just customized version of
 FSBrowser example included with Arduino core for ESP8266)
 The edit page http://espfs.local/edit can be optionally stored in flash via PROGMEM keyword (espWebServer.h), 
 so you can easily upload or edit your files only with a web browser (only 6Kb of flash).
 ESP32 platform could have a little bug to fix https://github.com/espressif/arduino-esp32/issues/3652
*/

#include <time.h>
#include <sys/time.h>   
#include <Ticker.h>
#include <FS.h>
#include <Wire.h>        
#include <GoogleSheet.h>

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

// WiFi setup
const char* ssid        =  "XXXXXXXXX";     // SSID WiFi network
const char* password    =  "XXXXXXXXX";     // Password  WiFi network       

struct tm timeinfo;
const char * getUpdatedtime(const uint32_t timeout);
bool fsOK = false;
#define FORMAT_IF_FAILED true   // Format filesystem if needed

const char *hostname = "espfs";
#include "espWebserver.h"

//GoogleFilelist sheetList;
GoogleSheetAPI mySheet(&fileSystem, "/sheet.json", nullptr );

// In your Drive can be stored hundres of sheets. 
// In this example we will keep sheets handled by application in a specific folder  
const char* APP_FOLDERNAME = "AppData";   // Google Drive online folder name

void setup(){
    Serial.begin(115200);

    // WiFi INIT
    startWiFi();

    // FILESYSTEM INIT
    startFilesystem();

    // Sync time with NTP. Blocking, but with timeout (0 == no timeout)
    const char* nowTime = getUpdatedtime(5000);    
    Serial.printf_P(PSTR("Time: %s\n"), nowTime); 

    // WEB SERVER INIT
    // Custom handlers for API testing functions
    server.on("/createSheet", HTTP_GET, testCreateSheet);       
    server.on("/listSheet", HTTP_GET, listSpreadsheetInFolder); 
    startWebServer();

    // Begin
    if (mySheet.begin()){
        
        // Authorization OK
        if (mySheet.getState() == GoogleDriveAPI::GOT_TOKEN){

            //Check if the "app folder" is present in your Drive and store the ID in class parameter
            String folderId = mySheet.searchFile(APP_FOLDERNAME);       
            mySheet.setAppFolderId(folderId);
            Serial.printf_P(PSTR("App folder present, id: %s\n"), folderId.c_str());   

            // A valid id founded for application folder (33 chars random string)
            if (folderId.length() >= 30 ) { 
                
                // Create a new spreadsheet if not exist, return the sheet id (44 random chars)
                String spreadSheetId = mySheet.searchFile("myTestSheet", folderId.c_str());             
                if(spreadSheetId.length() > 40){
                    mySheet.setSheetId(spreadSheetId);
                    Serial.printf_P(PSTR("myTestSheet present, id: %s\n"), spreadSheetId.c_str());
                }
                else {
                    String newSheetId = mySheet.newSpreadsheet("myTestSheet", folderId.c_str());
                    Serial.printf_P(PSTR("myTestSheet created, id: %s\n"), newSheetId.c_str());  
                    mySheet.setSheetId(newSheetId);
                    delay(1000);
                    
                    // Add header row  ['colA', 'colB', 'colC', .... ]
                    String row = F("['Timestamp', 'Max free heap', 'Max block size', '");
                    // Add a formula to the last column
                    row += F("=ARRAYFORMULA(IF(ISBLANK(A:A),, A:A/86400+date(1970,1,1)))");
                    row += F("']");
                    mySheet.appendRowToSheet( mySheet.getSheetId(), "Sheet1!A1",  row.c_str());
                }
                
            }
            else {
                Serial.println(F("Folder APP not present. Now it will be created."));
                mySheet.createFolder(APP_FOLDERNAME, "root");
                ESP.restart();
            }

        }
    }

    if(mySheet.getState() == GoogleOAuth2::INVALID){
        Serial.print(F("\n\n-------------------------------------"));
        Serial.print(F("\nGoogle says that your client is NOT VALID! You have to authorize the application.")); 
        Serial.print(F("\nFor instructions, check the page https://github.com/cotestatnt/Arduino-Google-API")); 
        Serial.print(F("\n-----------------------------------------\n\n"));
    }

}

    

void loop(){
    server.handleClient();
#ifndef ESP32
    MDNS.update();
#endif
    static uint32_t updateT;
    if(millis() - updateT > 30000){
        updateT = millis();
        
        // Append a new row to the sheet
        
#ifdef ESP32
        size_t free; size_t max; 
        free = heap_caps_get_free_size(0);
        max = heap_caps_get_largest_free_block(0);
#elif defined(ESP8266)        
        uint32_t free; uint16_t max; 
        ESP.getHeapStats(&free, &max, nullptr);     
#endif            
        char row[40];
        snprintf(row, 32, "[%ld, %d, %d]", time(nullptr), free, max);
        mySheet.appendRowToSheet( mySheet.getSheetId(), "Sheet1!A1",  row);
        printHeapStats();
    }
    yield();
}



void testCreateSheet(){
    // Check if sheet with name "Data" is already present in spreadsheet
    uint32_t sheetId = mySheet.hasSheet("Data", mySheet.getSheetId());
    if (sheetId > 0) {
        Serial.printf_P(PSTR("myTestSheet has sheet 'Data' with id: %d\n"), sheetId);   
    } 
    // If not present, create
    else {
        mySheet.newSheet("Data", mySheet.getSheetId());
    }
}


void listSpreadsheetInFolder(){
    String query = F(" and '");
    query += mySheet.getAppFolderId();
    query += F("' in parents");                
    Serial.println(F("\n\nSheets founded in folder app:"));
    mySheet.updateSheetList(query);
    mySheet.printSheetList();
}




////////////////////////////////  Flash filesystem  ////////////////////////////////////
void startFilesystem(){
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


////////////////////////////////  WiFi  /////////////////////////////////////////
void startWiFi(){
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

#ifdef ESP8266
    WiFi.hostname(hostname);  
#elif defined(ESP32) 
    WiFi.setHostname(hostname);
#endif 
    MDNS.begin(hostname);
    Serial.printf("Open http://%s.local/edit to edit or upload files\n", hostname);

     // Set timezone and NTP servers
#ifdef ESP8266
    configTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
#elif defined(ESP32) 
    configTzTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
#endif
}


////////////////////////////////  NTP Time  /////////////////////////////////////////
const char * getUpdatedtime(const uint32_t timeout) 
{
#define BUF_SIZE 40
    uint32_t start = millis();
    Serial.print("Sync time...");
    do {
        time_t now = time(nullptr); 
        timeinfo = *localtime(&now);
        delay(1);
    } while(millis() - start < timeout  && timeinfo.tm_year <= (1970 - 1900));

    Serial.println(" done."); 

    char * buffer = new char[BUF_SIZE];
    buffer[BUF_SIZE] = '\0';

    strftime (buffer, 40, "%A, %d/%m/%Y %H:%M:%S", &timeinfo);
    return (const char*) buffer;
}


////////////////////////////////  Heap memory info  /////////////////////////////////
void printHeapStats(){
#ifdef ESP32
    Serial.printf("\nTotal free: %6d - Max block: %6d\n", heap_caps_get_free_size(0), heap_caps_get_largest_free_block(0));
#elif defined(ESP8266)
    uint32_t free; uint16_t max; 
    ESP.getHeapStats(&free, &max, nullptr); 
    Serial.printf("\nTotal free: %5d - Max block: %5d\n", free, max);
#endif
}