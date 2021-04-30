#define HOSTNAME    "esp2sheet"     // Setting hostname, you can access to http://HOSTNAME.local instead fixed ip address
#define FOLDER_NAME  "SheetFolder"
#define S_FILENAME   "myTestSheet"
#define SHEETNAME    "datasource"

#define USE_CAPTIVE true
#ifdef USE_CAPTIVE
#include <DNSServer.h>
const byte DNS_PORT = 53;
DNSServer dnsServer;
#endif

// WiFi setup
const char* ssid  ;       // SSID WiFi network
const char* password;     // Password  WiFi network

#define NAME_LEN 16
String hostname = HOSTNAME;
String folderName = FOLDER_NAME;
String spreadSheetName = S_FILENAME;
String sheetName = SHEETNAME;
unsigned long updateInterval = 60000;           // Add row to sheet every x seconds

// Google API OAuth2.0 client setup default values (you can change later with setup webpage)
const char* client_id     =  "xxxxxxxxxxxx-f9g6btf4ip5ge3guv944q01qvoa2srhf.apps.googleusercontent.com";
const char* client_secret =  "xxxxxxxxxxxxxxxxxxxxxxxxx";
const char* api_key       =  "xxxxxxxx-kUJQOcFya2Ls7qMlofObXhsECs2e3i0";
const char* scopes        =  "https://www.googleapis.com/auth/drive.file "
                             "https://www.googleapis.com/auth/spreadsheets ";
const char* redirect_uri  =  "https://enyi3pe1qtnnvz9.m.pipedream.net";

// Timezone definition to get properly time from NTP server
#define MYTZ "CET-1CEST,M3.5.0,M10.5.0/3"

#include <FS.h>
#ifdef ESP8266
  #include <LittleFS.h>
  FS* fileSystem = &LittleFS;
#elif defined(ESP32)
  // Sse FFat or SPIFFS with ESP32
  #include <FFat.h>
  FS* fileSystem = &FFat;
#endif

#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <time.h>
#include <sys/time.h>
#include <GoogleSheet.h>
#include <ArduinoJson.h>

struct tm Time;
const char* formulaExample = "\"=IF(ISNUMBER(B:B); B:B - C:C;)\"";

WiFiClientSecure client;
GoogleSheetAPI mySheet(fileSystem, &client, nullptr );
bool apMode = true;
bool runAuthServer = false;

void saveApplicationConfig(){
    StaticJsonDocument<2048> doc;
    File file = FFat.open("/config.json", "w");
    doc["hostname"] = hostname;
    doc["Folder Name"] = folderName;
    doc["Spreadsheet Name"] = spreadSheetName;
    doc["Sheet Name"] = sheetName;
    doc["Update Time"] = updateInterval;
    serializeJsonPretty(doc, file);
    file.close();
    Serial.println(F("new file created with default values"));
    delay(1000);
    ESP.restart();
}


////////////////////////////////  Application setup  /////////////////////////////////////////
void loadApplicationConfig() {
    StaticJsonDocument<2048> doc;

    File file = FFat.open("/config.json", "r");
    if (file) {
        DeserializationError error = deserializeJson(doc, file);
        file.close();
        if (error) {
            Serial.println(F("Failed to deserialize file, may be corrupted"));
            Serial.println(error.c_str());
            saveApplicationConfig();
        }
        else {
            ssid  = doc["ssid"];
            password  = doc["password"];
            hostname = doc["hostname"].as<String>();
            folderName = doc["Folder Name"].as<String>();
            spreadSheetName = doc["Spreadsheet Name"].as<String>();
            sheetName = doc["Sheet Name"].as<String>();
            updateInterval = doc["Update Time"];

            Serial.println(hostname);
            Serial.println(folderName);
            Serial.println(spreadSheetName);
            Serial.println(sheetName);
        }
    }
    else {
        saveApplicationConfig();
    }
}


////////////////////////////////  WiFi  /////////////////////////////////////////
void startWiFi(){
    if(ssid != nullptr && password != nullptr) {
        Serial.printf("Connecting to %s\n", ssid);
        WiFi.setAutoReconnect(true);
        WiFi.setAutoConnect(true);
        WiFi.persistent(true);
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, password);

        // Try to connect to ssid, if it fail go back to AP_MODE
        uint32_t startTime = millis();
        while (WiFi.status() != WL_CONNECTED ){
            delay(500);
            Serial.print(".");
            if( millis() - startTime > 20000 )
                break;
        }

        if(WiFi.status() == WL_CONNECTED) {
            apMode = false;
            Serial.print("\nConnected! IP address: ");
            Serial.println(WiFi.localIP());
            // Set hostname, timezone and NTP servers
        #ifdef ESP8266
            WiFi.hostname(hostname);
            configTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
        #elif defined(ESP32)
            WiFi.setHostname(hostname.c_str());
            configTzTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
        #endif
            // Sync time with NTP. Blocking, but with timeout (0 == no timeout)
            getUpdatedtime(10000);
            char nowTime[30];
            strftime (nowTime, 30, "%d/%m/%Y - %X", &Time);
            Serial.printf("Synced time: %s\n", nowTime);
        }
    }
}


////////////////////////////////  NTP Time  /////////////////////////////////////////
void getUpdatedtime(const uint32_t timeout)
{
    uint32_t start = millis();
    Serial.print("Sync time...");
    do {
        time_t now = time(nullptr);
        Time = *localtime(&now);
        delay(1);
    } while(millis() - start < timeout  && Time.tm_year <= (1970 - 1900));
    Serial.println(" done.");
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


void setup(){
    delay(2000);
    Serial.begin(115200);
    Serial.println("\n\nSTART\n");

    // Mount filesystem and load saved configuration file
    Serial.println("Mount filesystem");
    if(FFat.begin() == false){
        Serial.println("Error on mounting filesystem. Now it will be formatted, please restart!");
        FFat.format();
        return;
    }
    loadApplicationConfig();

    // Set Google API certificate
    client.setCACert(google_cert);

    // WiFi INIT
    startWiFi();

    if (WiFi.isConnected()) {
        // Begin Google Sheet handling (to store tokens and configuration, filesystem must be mounted before)
        if (mySheet.begin(client_id, client_secret, scopes, api_key, redirect_uri)){

            // Authorization OK when we have a valid token
            if (mySheet.getState() == GoogleDriveAPI::GOT_TOKEN){

                //Check if the "app folder" is present in your Drive and store the ID in class parameter
                String folderId = mySheet.searchFile(folderName);
                mySheet.setAppFolderId(folderId);
                Serial.printf("App folder present, id: %s\n", folderId.c_str());

                // An id founded for application folder (33 chars random string)
                if (folderId.length() >= 30 ) {

                    // Create a new spreadsheet if not exist, return the sheet id (44 random chars)
                    String spreadSheetId = mySheet.searchFile("myTestSheet", folderId.c_str());
                    if(spreadSheetId.length() > 40){
                        mySheet.setSheetId(spreadSheetId);
                        Serial.printf("myTestSheet present, id: %s\n", spreadSheetId.c_str());
                    }
                    else {
                        String newSheetId = mySheet.newSpreadsheet(spreadSheetName, sheetName, folderId);
                        Serial.printf("myTestSheet created, id: %s\n", newSheetId.c_str());
                        mySheet.setSheetId(newSheetId);
                        delay(1000);

                        // Add header row  ['colA', 'colB', 'colC', .... ]
                        String row = "['Timestamp', 'Max free heap', 'Max block size', 'Difference']";
                        String range = sheetName;
                        range += "!A1";
                        mySheet.appendRowToSheet( mySheet.getSheetId(), range.c_str(),  row.c_str());
                    }
                }
                else {
                    Serial.println(F("Folder APP not present. Now it will be created and then ESP will restart"));
                    mySheet.createFolder(folderName.c_str(), "root");
                    ESP.restart();
                }
            }
        }

        if(mySheet.getState() == GoogleOAuth2::INVALID){
            Serial.print(F("\n\n-------------------------------------------------------------------------------"));
            Serial.print(F("\nGoogle says that your client is NOT VALID! You have to authorize the application."));
            Serial.print(F("\nFor instructions, check the page https://github.com/cotestatnt/Arduino-Google-API\n"));
            Serial.print(F("\nOpen this link with your browser to authorize this appplication, then restart.\n\n"));
            Serial.printf("\nhttp://%s.local/\n", HOSTNAME);
            Serial.print(F("\n---------------------------------------------------------------------------------\n\n"));
            runAuthServer = true;
        }
    }

    runAuthServer = true;
    if(apMode || runAuthServer ) {
        if(apMode) {
            Serial.print("\nStart Access point mode");
            WiFi.mode(WIFI_AP);
            WiFi.softAP(hostname.c_str());
            // if DNSServer is started with "*" for domain name, it will reply with provided IP to all DNS request
            dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
            Serial.printf("\ndns server started with ip: %s\n", WiFi.softAPIP().toString().c_str());
            dnsServer.start(DNS_PORT, F("*"), WiFi.softAPIP());
        }
        WiFi.setHostname(hostname.c_str());
        if (MDNS.begin(hostname.c_str())) {
            Serial.println("MDNS responder started.\nOn Window you need to install service like Bonjour");
            Serial.printf("You should be able to connect with address\t http://%s.local/\n", hostname.c_str());
            // Add service to MDNS-SD
            MDNS.addService("http", "tcp", 80);
        }
        mySheet.authServer->addOption(FFat, "hostname", hostname);
        mySheet.authServer->addOption(FFat, "Folder Name", folderName);
        mySheet.authServer->addOption(FFat, "Spreadsheet Name", spreadSheetName);
        //mySheet.authServer->addOption(FFat, "One spreadsheet for day", false);
        mySheet.authServer->addOption(FFat, "Sheet Name", sheetName);
        mySheet.authServer->addOption(FFat, "Update Time", updateInterval);
        mySheet.startAuthServer();
        while(apMode) {
            dnsServer.processNextRequest();
            mySheet.authServer->run();
            #ifdef ESP8266
            MDNS.update();
            #endif
            yield();
            apMode = WiFi.status() != WL_CONNECTED;
        }
        dnsServer.stop();
    }
}


void loop() {
    if(runAuthServer)
        mySheet.authServer->run();

    if(WiFi.status() == WL_CONNECTED) {
        // Append a new row to the sheet every 30s
        static uint32_t updateT;
        if(millis() - updateT > updateInterval){
            updateT = millis();
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
            if( mySheet.appendRowToSheet( mySheet.getSheetId(), range.c_str(), row) ) {
                Serial.printf("New row appended to %s::%s succesfully\n", spreadSheetName.c_str(), sheetName.c_str());
            }
            printHeapStats();
        }
    }
    else {
        static uint32_t reconTime;
        if(millis() - reconTime > 5000) {
            WiFi.begin();
            Serial.print("*-");
            reconTime = millis();
        }
    }
}
