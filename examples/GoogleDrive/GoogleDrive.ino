/*
 Name:	      GooglDrive.ino
 Created:     26/11/2020
 Author:      Tolentino Cotesta <cotestatnt@yahoo.com>
 Description: this example show how to store and search files using Drive API. Work with both ESP8666 and ESP32.
 The uploadToDrive () function is called from the /drive.htm web page, but of course it can be called with any condition.
 
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
#include <BMx280I2C.h>                
#include <GoogleDrive.h>

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
const char* ssid        =  "ssid";     // SSID WiFi network
const char* password    =  "xxxx";     // Password  WiFi network       

/* https://developers.google.com/identity/protocols/OAuth2ForDevices#creatingcred */
const char* CLIENT_ID     =  "xxxxxxxxxxxx-6d2s2ne5v1eoapfxxxxxxxxxxxxx.apps.googleusercontent.com";
const char* CLIENT_SECRET =  "xxxxxxxxxxxx-xxxxxxxxxxx";
const char* SCOPES        =  "https://www.googleapis.com/auth/drive.file email profile";

Ticker saveMeasure, updateGList;
struct tm timeinfo;
const char * getUpdatedtime(const uint32_t timeout);
bool fsOK = false;
#define FORMAT_IF_FAILED true   // Format filesystem if needed

const char *hostname = "espfs";
#include "espWebserver.h"

float temperature, pressure, altitude;  // Create the temperature, pressure and altitude variables
BMx280I2C bmx280(0x76);                 // Create a BMx280I2C object using the I2C interface with Address 0x76

GoogleFilelist driveList;
GoogleDriveAPI myDrive(&fileSystem, &driveList );

// With Google Drive, we need the id of folder and files in order to access and work with
// In this example, all files managed from the device will be stored in folder APP_FOLDERNAME
// Temperature e pressure datas will be written in a daily text file
const char* APP_FOLDERNAME = "appData";   // Google Drive online folder name
const char* dataFolderName = "/data";     // Local folder for store data files
#define MAX_NAME_LEN 16
char dataFileName[MAX_NAME_LEN];                    // ex. "20201025.txt"
 

void setup(){
    Serial.begin(115200);

    // Sensor INIT
    startBMP280();

    // WiFi INIT
    startWiFi();

    // FILESYSTEM INIT
    startFilesystem();

    // Sync time with NTP. Blocking, but with timeout (0 == no timeout)
    const char* nowTime = getUpdatedtime(5000);    
    Serial.print("Time: "); 
    Serial.println(nowTime); 

    // Add custom handler for webserver
    server.on("/sendToDrive", HTTP_GET, uploadToDrive);
    server.on("/getData", HTTP_GET, [](){
        char path[10 + MAX_NAME_LEN];
        snprintf(path, 30, "%s/%s%c", dataFolderName, dataFileName, '\0' );
        Serial.printf("Sent data file to client - %s\n", path);
        handleFileRead(path);    
    });

    // WEB SERVER INIT
    startWebServer();

    // Print some information about heap memory before use init the library
    printHeapStats();
	
    // Begin, after wait authorize if necessary (with 60s timeout)
	if (myDrive.begin(CLIENT_ID, CLIENT_SECRET, SCOPES)){
		uint32_t timeout = millis();
		while (myDrive.getState() != GoogleDriveAPI::GOT_TOKEN){
			// Wait for user authorization on https//www.google.come/device
			delay(10000);
			if(millis() - timeout > 60000) break;
		}

		/*  
		 *  Check if the "app folder" is present in your Drive
		 *  The method searchFile() will also populate the local filelist with all files 
		 *  present online, so we can quick access to the file id without interrogate
		 *  google server each time we need.
		*/
		if (myDrive.getState() == GoogleDriveAPI::GOT_TOKEN){
			const char* appFolderId = myDrive.searchFile(APP_FOLDERNAME);
			
			if (appFolderId != nullptr) {
				Serial.println("\n\nFiled created with this app:");
				driveList.printList();
			}
			else {
				Serial.println("Folder APP not present. Now it will be created.");
				myDrive.createAppFolder(APP_FOLDERNAME);
			}
		}
	}

    // Update name of the data file 
    snprintf(dataFileName, MAX_NAME_LEN, "%04d%02d%02d.txt", timeinfo.tm_year+1900, timeinfo.tm_mon+1, timeinfo.tm_mday );
    // Append to the file the first measure on reboot
    appendMeasurement();

    // Write to daily data file once a minute
    saveMeasure.attach(60, appendMeasurement);

    // Update file list every 120 seconds
    updateGList.attach(120, [](){
        myDrive.updateList();
    });
}

    

void loop(){
    server.handleClient();
#ifndef ESP32
    MDNS.update();
#endif
    // Get temperature and barometric pressure
    bmx280.measure();
    if(bmx280.hasValue()){
        temperature = bmx280.getTemperature();
        pressure = bmx280.getPressure64()/100;
    }
}


void appendMeasurement(){
    // Update name of the data file if necessary
    snprintf(dataFileName, MAX_NAME_LEN, "%04d%02d%02d.txt", timeinfo.tm_year+1900, timeinfo.tm_mon+1, timeinfo.tm_mday );
    char path[10 + MAX_NAME_LEN];
    snprintf(path, 30, "%s/%s", dataFolderName, dataFileName );
    if(fileSystem.exists(path)){
        getLocalTime(&timeinfo);
        char dataBuf[30];
        sprintf(dataBuf, "%02d:%02d:%02d; %d.%d; %d.%d",
            timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
            (int)temperature, (int)(temperature * 100)%100,
            (int)pressure, (int)(pressure * 100)%100);
    
        File file = fileSystem.open(path, FILE_APPEND);
        file.println(dataBuf);
        file.close();
        Serial.print(F("\nAppend new data to file: "));
        Serial.println(dataBuf);
    }
    else
        createDayFile();    

    printHeapStats();   // Print information about heap memory
}

void uploadToDrive(){
    String filename = dataFileName;
    String foldername = APP_FOLDERNAME; 

    // Optional params    
    if (server.hasArg("file"))
        filename = server.arg("file");        
    
    if (server.hasArg("folder"))
        foldername = server.arg("folder"); 

    const char* id = nullptr;
    const char* folderid = nullptr;

    // Search the Google IDs in the app filelist
    Serial.println("\n\nFile created with this app:");
    driveList.printList();

    for(uint8_t i=0; i<myDrive.getNumFiles(); i++){
        if( filename.equals(myDrive.getFileName(i)) )
            id = myDrive.getFileId(i);               
        if( foldername.equals(myDrive.getFileName(i)) )
            folderid = myDrive.getFileId(i);   
    }

    char path[10 + MAX_NAME_LEN];
    snprintf(path, 30, "%s/%s%c", dataFolderName, dataFileName, '\0' );

    String resp;
    if(id != nullptr) {        
        Serial.println("\n\nFile present in app folder. Call update method");
        Serial.println(path);
        resp = myDrive.uploadFile(path, id, true);
    }
    else {        
        Serial.println("\n\nFile not present in app folder. Call upload method");
        Serial.println(path);
        resp =  myDrive.uploadFile(path, folderid, false);
    }

    if(resp != nullptr){  
        String text =  F("File uploaded to Drive with id "); 
        text += id;
        server.send(200, "text/plain", text);
        Serial.println(text);
    }
    else{
        server.send(200, "text/plain", "Error");  
        Serial.print("\nError. File not uploaded correctly.");
    }
}


void createDayFile(){
    // Create a file for each different day
    Serial.print("Filesystem free bytes: ");
#ifdef ESP32
    Serial.println(fileSystem.freeBytes());
#elif defined(ESP8266)
    Serial.println(fileSystem.totalBytes() - fileSystem.usedBytes() );
#endif
    
    char path[10 + MAX_NAME_LEN];
    snprintf(path, 30, "%s/%s%c", dataFolderName, dataFileName, '\0' );

    if( !fileSystem.exists(dataFolderName)){
        fileSystem.mkdir(dataFolderName);   
    }

    // Storage of a complete day data need at least 32Kb  (one measure once a minute)    
    File dataFile =  fileSystem.open(path, FILE_WRITE);
    if (!dataFile) {
        Serial.print(F("There was an error opening the file for writing - "));
        Serial.println(path);
    }
    dataFile.close();
}


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
        File root = fileSystem.open("/");
        File file = root.openNextFile();
        while (file){
            const char* fileName = file.name();
            size_t fileSize = file.size();
            Serial.printf("FS File: %s, size: %lu\n", fileName, (long unsigned)fileSize);
            file = root.openNextFile();
        }
        Serial.println();
    }
    createDayFile();  
}



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

    WiFi.setHostname(hostname);
    MDNS.begin(hostname);
    Serial.printf("Open http://%s.local/edit to edit or upload files\n", hostname);

     // Set timezone and NTP servers
#ifdef ESP8266
    configTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
#elif defined(ESP32) 
    configTzTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
#endif
}

void startBMP280(){
    Wire.begin(21, 22);
    if (!bmx280.begin()){
        Serial.println("begin() failed. check your BMx280 Interface and I2C Address.");        
    }
    bmx280.resetToDefaults();
    bmx280.writeOversamplingPressure(BMx280MI::OSRS_P_x16);
    bmx280.writeOversamplingTemperature(BMx280MI::OSRS_T_x16);

    //start a measurement
    bmx280.measure();
    delay(500);
    if(bmx280.hasValue()){
        temperature = bmx280.getTemperature();
        pressure = bmx280.getPressure64()/100;
        Serial.print("Pressure: "); Serial.println(pressure);
        Serial.print("Temperature: "); Serial.println(temperature);
    }
}


const char * getUpdatedtime(const uint32_t timeout) 
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

void printHeapStats(){
#ifdef ESP32
    Serial.printf("Free heap: %6d - Max block: %6d\n", heap_caps_get_free_size(0), heap_caps_get_largest_free_block(0));
#elif defined(ESP8266)
    uint32_t free; uint16_t max; uint8_t frag; 
    ESP.getHeapStats(&free, &max, &frag); 
    Serial.printf("Free heap: %5d - Max block: %5d\n", free, max);
#endif
}
