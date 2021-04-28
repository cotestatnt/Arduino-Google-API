#ifndef AuthServer_H
#define AuthServer_H


#define DEBUG_MODE_WS true
#if DEBUG_MODE_WS
#define DebugPrint(x) Serial.print(x)
#define DebugPrintln(x) Serial.println(x)
#else
#define DebugPrint(x)
#define DebugPrintln(x)
#endif

#include <Arduino.h>
#include <FS.h>
#include <ArduinoJson.h>

#include <DNSServer.h>
#include <memory>
#include <typeinfo>

#define INCLUDE_EDIT_HTM
#ifdef INCLUDE_EDIT_HTM
#include "edit_htm.h"
#endif

#include "setup_htm.h"

#ifdef ESP8266
    #include <ESP8266WiFi.h>
    #include <ESP8266WebServer.h>
    #include <ESP8266mDNS.h>
#elif defined(ESP32)
    #include <WebServer.h>
    #include <WiFi.h>
    #include <ESPmDNS.h>
#endif

#define DBG_OUTPUT_PORT Serial

enum { MSG_OK, CUSTOM, NOT_FOUND, BAD_REQUEST, ERROR };
#define TEXT_PLAIN "text/plain"
#define FS_INIT_ERROR "FS INIT ERROR"
#define FILE_NOT_FOUND "FileNotFound"

class AuthServer{

public:
    AuthServer(fs::FS* fs);
    void begin();
    void run();

    // Add custom option to config webpage
    template <typename T>
    void addOption(fs::FS &fs, const char* label, T value) {
        StaticJsonDocument<2048> doc;
        File file = fs.open("/config.json", "r");
        if (file) {
            // If file is present, load actual configuration
            DeserializationError error = deserializeJson(doc, file);
            if (error) {
                Serial.println(F("Failed to deserialize file, may be corrupted"));
                Serial.println(error.c_str());
                file.close();
                return;
            }
            file.close();
        }
        else {
            Serial.println(F("File not found, will be created new configuration file"));
        }
        doc[label] = static_cast<T>(value);
        file = fs.open("/config.json", "w");
        if (serializeJsonPretty(doc, file) == 0) {
            Serial.println(F("Failed to write to file"));
        }
        file.close();
    }

private:

#ifdef ESP8266
    std::unique_ptr<ESP8266WebServer> server;
#elif defined(ESP32)
    std::unique_ptr<WebServer>        server;
#endif

    fs::FS*     m_filesystem;
    File        m_uploadFile;
    bool        m_fsOK = false;

    // Default handler for all URIs not defined above, use it to read files from filesystem
    void doWifiConnection();
    void doRestart();
    void replyOK();
    void getIpAddress();
    void handleRequest();
    void handleWebApp();
    bool handleFileRead( const String &path);
    void handleFileUpload();
    void replyToCLient(int msg_type, const char* msg);
    const char* getContentType(const char* filename);
    const char* checkForUnsupportedPath(String &filename);
    void setCrossOrigin();
    void handleScanNetworks();

    // edit page, in usefull in some situation, but if you need to provide only a web interface, you can disable
#ifdef INCLUDE_EDIT_HTM
    void handleGetEdit();
    void handleFileCreate();
    void handleFileDelete();
    void handleStatus();
    void handleFileList();
#endif // INCLUDE_EDIT_HTM
};

#endif