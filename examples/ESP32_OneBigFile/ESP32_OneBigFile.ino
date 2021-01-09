#include <time.h>
#include <sys/time.h>   
#include <Ticker.h>
#include <FS.h>             
#include <GoogleDrive.h>

// Timezone definition to get properly time from NTP server
#define MYTZ "CET-1CEST,M3.5.0,M10.5.0/3"

#include <SPIFFS.h>
const char* fsName = "SPIFFS"; 
#define fileSystem SPIFFS

/*
#include <FFat.h>
const char* fsName = "FFat"; 
#define fileSystem FFat
*/


// WiFi setup
const char* ssid        =  "xxxxxxx";     // SSID WiFi network
const char* password    =  "xxxxxxx";     // Password  WiFi network       

/* https://developers.google.com/identity/protocols/OAuth2ForDevices#creatingcred */
const char* CLIENT_ID     =  "xxxxxxxxxxxx-6d2s2ne5v1eoapfss5t1iooo16gp5gk7.apps.googleusercontent.com";
const char* CLIENT_SECRET =  "JqfXMOkBjXXXX_4unmi4XXXX";
const char* SCOPES        =  "https://www.googleapis.com/auth/drive.file email profile";

struct tm timeinfo;
const char * getUpdatedtime(const uint32_t timeout);
bool fsOK = false;
#define FORMAT_IF_FAILED true   // Format filesystem if needed

const char *hostname = "espfs";
#include "espWebserver.h"

GoogleDriveAPI myDrive(&fileSystem, NULL );

// With Google Drive, we need the id of folder and files in order to access and work with
// In this example, all files managed from the device will be stored in folder APP_FOLDERNAME
// Temperature e pressure datas will be written in a daily text file
const char* APP_FOLDERNAME = "AppData";   // Google Drive online folder name
const char* dataFolderName = "";          // Local folder for store data files ("" root folder)

#define MAX_NAME_LEN 16
String dataFileName = "audio.wav";
 
void setup(){
    Serial.begin(115200);

    // WiFi INIT
    startWiFi();

    // FILESYSTEM INIT
    startFilesystem();

    // Sync time with NTP. Blocking, but with timeout (0 == no timeout)
    const char* nowTime = getUpdatedtime(5000);    
    Serial.printf("Time: %s\n", nowTime); 

    // Add custom handler for webserver
    server.on("/sendToDrive", HTTP_GET, uploadToDrive);

    // WEB SERVER INIT
    startWebServer();

    // Begin, after wait authorize if necessary (with 60s timeout)
    if (myDrive.begin(CLIENT_ID, CLIENT_SECRET, SCOPES)){
        uint32_t timeout = millis();
        while (myDrive.getState() != GoogleDriveAPI::GOT_TOKEN){

            // Notify user about pending authorization required https//www.google.come/device with user_code
            if(myDrive.getState() == GoogleDriveAPI::REQUEST_AUTH){
                Serial.println(F("\nApplication need to be authorized!"));
                Serial.print(F("Open with a browser the address < https//www.google.come/device > and insert this confirmation code "));
                Serial.println(myDrive.getUserCode());
            }
            
            // Wait for user authorization on https//www.google.come/device
            delay(10000);
            if(millis() - timeout > 60000) break;
        }


        // Check if the "app folder" is present in your Drive
        if (myDrive.getState() == GoogleDriveAPI::GOT_TOKEN){
            String appFolderId =  myDrive.searchFile(APP_FOLDERNAME);

            if (appFolderId.length() < 30 ) {  
                Serial.println("Folder APP not present. Now it will be created.");
                myDrive.createFolder(APP_FOLDERNAME, "root");
            } 
            else {
                // Save the folder id for easy retrieve when needed
                myDrive.setAppFolderId(appFolderId);
                Serial.printf("App folder id: %s\n", appFolderId.c_str());
            }
        }
    }

}

    

void loop(){
    server.handleClient();
}


void uploadToDrive(){

    String localPath = dataFolderName;
    localPath += "/";
    localPath += dataFileName;

    // Check if file is already loaded on Drive
    String fileid = myDrive.searchFile(dataFileName);

    String uploadedId;
    if(fileid.length() > 30) {
        Serial.print("\n\nFile present in app folder. \nCall update method for ");
        Serial.println(localPath);
        uploadedId = myDrive.uploadFile(localPath, fileid, true);
    }
    else {        
        Serial.print("\n\nFile not present in app folder. Call upload method for ");
        Serial.println(localPath);
        String appFolderId = myDrive.getAppFolderId();
        uploadedId = myDrive.uploadFile(localPath, appFolderId, false);
    }

    if(uploadedId.length()){  
        String text =  F("File uploaded to Drive with id "); 
        text += uploadedId;
        server.send(200, "text/plain", text);
        Serial.println(text);
    }
    else{
        server.send(200, "text/plain", "Error");  
        Serial.print("\nError. File not uploaded correctly.");
    }

}


void startFilesystem(){
    // FILESYSTEM INIT

    //fileSystem.format();
    fsOK = fileSystem.begin(FORMAT_IF_FAILED);
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
    configTzTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");

}


const char * getUpdatedtime(const uint32_t timeout) {
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