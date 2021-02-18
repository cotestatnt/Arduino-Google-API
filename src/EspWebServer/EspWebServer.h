#ifndef ESPWEBSERVER_H
#define ESPWEBSERVER_H

#include <Arduino.h>
#include <FS.h>

#include <DNSServer.h>
#include <memory>

#define INCLUDE_EDIT_HTM
#ifdef INCLUDE_EDIT_HTM
#include "edit_htm.h"
#endif

#include "token_htm.h"

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

class EspWebServer{

public:
    EspWebServer(fs::FS* fs);
    void begin();
    void run();

private:
#ifdef ESP8266
    std::unique_ptr<ESP8266WebServer> server;
#else
    std::unique_ptr<WebServer>        server;
#endif

    const char* FS_Type;
    fs::FS*     m_filesystem;
    File        m_uploadFile;
    bool        m_fsOK = false;

    // Default handler for all URIs not defined above, use it to read files from filesystem
    void replyOK();
    void handleRequest();
    void handleWebApp();
    bool handleFileRead( const String &path);
    void handleFileUpload();
    void replyToCLient(int msg_type, const char* msg);
    const char* getContentType(const char* filename);
    const char* checkForUnsupportedPath(String &filename);
    void setCrossOrigin();
    // edit page, in usefull in some situation, but if you need to provide only a web interface, you can disable

#ifdef ESP32
    void updateFsInfo(FSInfo* fsInfo);
#endif
    void handleGetEdit();
    void handleFileCreate();
    void handleFileDelete();
    void handleStatus();
    void handleFileList();
};

#endif