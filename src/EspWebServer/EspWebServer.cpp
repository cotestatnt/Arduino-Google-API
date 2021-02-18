#include "EspWebServer.h"

EspWebServer::EspWebServer(fs::FS *fs){
    m_filesystem = fs;
}


const char* EspWebServer::getContentType(const char* filename) {
    if (server->hasArg("download"))
        return PSTR("application/octet-stream");
    else if (strstr(filename, ".htm"))
        return PSTR("text/html");
    else if (strstr(filename, ".html"))
        return PSTR("text/html");
    else if (strstr(filename, ".css"))
        return PSTR("text/css");
    else if (strstr(filename, ".js"))
        return PSTR("application/javascript");
    else if (strstr(filename, ".png"))
        return PSTR("image/png");
    else if (strstr(filename, ".gif"))
        return PSTR("image/gif");
    else if (strstr(filename, ".jpg"))
        return PSTR("image/jpeg");
    else if (strstr(filename, ".ico"))
        return PSTR("image/x-icon");
    else if (strstr(filename, ".xml"))
        return PSTR("text/xml");
    else if (strstr(filename, ".pdf"))
        return PSTR("application/x-pdf");
    else if (strstr(filename, ".zip"))
        return PSTR("application/x-zip");
    else if (strstr(filename, ".gz"))
        return PSTR("application/x-gzip");
    return PSTR("text/plain");
}

void EspWebServer::begin(){

    File root = m_filesystem->open("/", "r");
    if(root) {
        m_fsOK = true;
        File root = m_filesystem->open("/", "r");
        File file = root.openNextFile();
        while (file){
            const char* fileName = file.name();
            size_t fileSize = file.size();
            Serial.printf("FS File: %s, size: %lu\n", fileName, (long unsigned)fileSize);
            file = root.openNextFile();
        }
        Serial.println();
    }

#ifdef ESP8266
    server.reset(new ESP8266WebServer(80));
#else
    server.reset(new WebServer(80));
#endif

#ifdef INCLUDE_EDIT_HTM
    server->on("/status", HTTP_GET, std::bind(&EspWebServer::handleStatus, this));
    server->on("/list", HTTP_GET, std::bind(&EspWebServer::handleFileList, this));
    server->on("/edit", HTTP_GET, std::bind(&EspWebServer::handleGetEdit, this));
    server->on("/edit", HTTP_PUT, std::bind(&EspWebServer::handleFileCreate, this));
    server->on("/edit", HTTP_DELETE, std::bind(&EspWebServer::handleFileDelete, this));

#endif
    // Handle request for token generation page
    server->onNotFound(std::bind(&EspWebServer::handleRequest, this));
    server->on("/token", HTTP_GET, std::bind(&EspWebServer::handleWebApp, this));
    // Upload file
    // - first callback is called after the request has ended with all parsed arguments
    // - second callback handles file upload at that location
    server->on("/edit",  HTTP_POST, std::bind(&EspWebServer::replyOK, this), std::bind(&EspWebServer::handleFileUpload, this));
#ifdef ESP32
    server->enableCrossOrigin(true);
#endif
    server->begin();
    Serial.println("Server started");
}

void EspWebServer::run(){
    server->handleClient();
}

void EspWebServer::setCrossOrigin(){
    server->sendHeader(F("Access-Control-Allow-Origin"), F("*"));
    server->sendHeader(F("Access-Control-Max-Age"), F("600"));
    server->sendHeader(F("Access-Control-Allow-Methods"), F("PUT,POST,GET,OPTIONS"));
    server->sendHeader(F("Access-Control-Allow-Headers"), F("*"));
};

void EspWebServer::handleWebApp(){
    server->sendHeader(PSTR("Content-Encoding"), "gzip");
    server->send_P(200, "text/html", WEBPAGE_HTML, WEBPAGE_HTML_SIZE);
}

void EspWebServer::handleRequest(){
    if (!m_fsOK){
        replyToCLient(ERROR, PSTR(FS_INIT_ERROR));
        return;
    }
#ifdef ESP32
    String _url = WebServer::urlDecode(server->uri());
#elif defined(ESP8266)
    String _url = ESP8266WebServer::urlDecode(server->uri());
#endif
    // First try to find and return the requested file from the filesystem,
    // and if it fails, return a 404 page with debug information
    if (handleFileRead(_url))
        return;
    else
        replyToCLient(NOT_FOUND, PSTR(FILE_NOT_FOUND));
}


/*
    Read the given file from the filesystem and stream it back to the client
*/
bool EspWebServer::handleFileRead( const String &uri) {
    DBG_OUTPUT_PORT.printf_P(PSTR("handleFileRead: %s\n"), uri.c_str());

    // // File not found, try gzip version
    // if (!m_filesystem->exists(path)){
    //     path += ".gz";
    // }

    Serial.println(uri);

    if (m_filesystem->exists(uri)) {
        const char* contentType = getContentType(uri.c_str());
        File file = m_filesystem->open(uri, "r");
        if (server->streamFile(file, contentType) != file.size()) {
            DBG_OUTPUT_PORT.println(PSTR("Sent less data than expected!"));
        }
        file.close();
        return true;
    }
    return false;
}


/*
    Handle a file upload request
*/
void EspWebServer::handleFileUpload() {
    if (server->uri() != "/edit") {
        return;
    }
    HTTPUpload& upload = server->upload();
    if (upload.status == UPLOAD_FILE_START) {
        String filename = upload.filename;
        // Make sure paths always start with "/"
        if (!filename.startsWith("/")) {
            filename = "/" + filename;
        }

        Serial.println("check name");
        if (strlen(checkForUnsupportedPath(filename)) > 0) {
            replyToCLient(ERROR, PSTR("INVALID FILENAME"));
            return;
        }

        DBG_OUTPUT_PORT.printf_P(PSTR("handleFileUpload Name: %s\n"), filename.c_str());
        m_uploadFile = m_filesystem->open(filename, "w");
        if (!m_uploadFile) {
            replyToCLient(ERROR, PSTR("CREATE FAILED"));
            return;
        }
        DBG_OUTPUT_PORT.printf_P(PSTR("Upload: START, filename: %s\n"), filename.c_str());
    }
    else if (upload.status == UPLOAD_FILE_WRITE) {
        if (m_uploadFile) {
            size_t bytesWritten = m_uploadFile.write(upload.buf, upload.currentSize);
            if (bytesWritten != upload.currentSize) {
                replyToCLient(ERROR, PSTR("WRITE FAILED"));
                return;
            }
        }
        DBG_OUTPUT_PORT.printf_P(PSTR("Upload: WRITE, Bytes: %d\n"), upload.currentSize);
    }
    else if (upload.status == UPLOAD_FILE_END) {
        if (m_uploadFile) {
            m_uploadFile.close();
        }
        DBG_OUTPUT_PORT.printf_P(PSTR("Upload: END, Size: %d\n"), upload.totalSize);
    }
}


void EspWebServer::replyToCLient(int msg_type=0, const char* msg="") {
    server->sendHeader("Access-Control-Allow-Origin", "*");
    switch (msg_type){
        case OK:
            server->send(200, FPSTR(TEXT_PLAIN), "");
            break;
        case CUSTOM:
            server->send(404, FPSTR(TEXT_PLAIN), msg);
            break;
        case NOT_FOUND:
            server->send(404, FPSTR(TEXT_PLAIN), msg);
            break;
        case BAD_REQUEST:
            server->send(400, FPSTR(TEXT_PLAIN), msg );
            break;
        case ERROR:
            server->send(500, FPSTR(TEXT_PLAIN), msg );
            break;
    }
}

void EspWebServer::replyOK(){
    replyToCLient(OK, "");
}

/*
    Checks filename for character combinations that are not supported by FSBrowser (alhtough valid on SPIFFS).
    Returns an empty String if supported, or detail of error(s) if unsupported
*/
const char* EspWebServer::checkForUnsupportedPath(String &filename) {

    String error = String();
    if (!filename.startsWith("/")) {
        error += PSTR(" !! NO_LEADING_SLASH !! ");
    }
    if (filename.indexOf("//") != -1) {
        error += PSTR(" !! DOUBLE_SLASH !! ");
    }
    if (filename.endsWith("/")) {
        error += PSTR(" ! TRAILING_SLASH ! ");
    }
    Serial.println(filename);
    Serial.println(error);
    return error.c_str();
}


// edit page, in usefull in some situation, but if you need to provide only a web interface, you can disable
#ifdef INCLUDE_EDIT_HTM

#ifdef ESP32
void EspWebServer::updateFsInfo(FSInfo* fsInfo) {

#ifdef _SPIFFS_H_
    fsInfo->totalBytes = SPIFFS.totalBytes();
    fsInfo->usedBytes = SPIFFS.usedBytes();
    Serial.println("SPIFFS");
#elif defined(_FFAT_H_)
    // fsInfo->totalBytes = FFat.totalBytes();
    // fsInfo->usedBytes = FFat.totalBytes() - FFat.freeBytes();
    Serial.println("FFat");
#endif

}
#endif

/*
    Return the list of files in the directory specified by the "dir" query string parameter.
    Also demonstrates the use of chuncked responses.
*/
void EspWebServer::handleFileList() {
    if (!server->hasArg("dir")) {
        server->send(500, "text/plain", "BAD ARGS");
        return;
    }

    String path = server->arg("dir");
    DBG_OUTPUT_PORT.println("handleFileList: " + path);

    File root = m_filesystem->open(path, "r");
    path = String();
    String output = "[";
    if(root.isDirectory()){
        File file = root.openNextFile();
        String filename = file.name();
        // Remove the first "/" if present (depends from filesystem used)
        if(filename[0] == '/')
            filename = filename.substring(1);
        while(file){
            if (output != "[") {
                output += ',';
            }
            output += "{\"type\":\"";
            output += (file.isDirectory()) ? "dir" : "file";
            output +=  "\",\"size\":\"";
            output += file.size();
            output += "\",\"name\":\"";
            output += filename;
            output += "\"}";
            file = root.openNextFile();
        }
    }
    output += "]";
    server->send(200, "text/json", output);
}


/*
    Handle the creation/rename of a new file
    Operation      | req.responseText
    ---------------+--------------------------------------------------------------
    Create file    | parent of created file
    Create folder  | parent of created folder
    Rename file    | parent of source file
    Move file      | parent of source file, or remaining ancestor
    Rename folder  | parent of source folder
    Move folder    | parent of source folder, or remaining ancestor
*/
void EspWebServer::handleFileCreate(){
    String path = server->arg("path");
    if (path.isEmpty()) {
        replyToCLient(BAD_REQUEST, PSTR("PATH ARG MISSING"));
        return;
    }
    if (path == "/") {
        replyToCLient(BAD_REQUEST, PSTR("BAD PATH"));
        return;
    }
    if (m_filesystem->exists(path)) {
        replyToCLient(BAD_REQUEST, PSTR("PATH FILE EXISTS"));
        return;
    }

    String src = server->arg("src");
    if (src.isEmpty()) {
        // No source specified: creation
        DBG_OUTPUT_PORT.printf_P(PSTR("handleFileCreate: %s\n"), path.c_str());
        if (path.endsWith("/")) {
            // Create a folder
            path.remove(path.length() - 1);
            if (!m_filesystem->mkdir(path)) {
                replyToCLient(ERROR, PSTR("MKDIR FAILED"));
                return;;
            }
        }
        else {
            // Create a file
            File file = m_filesystem->open(path, "w");
            if (file) {
                file.write(0);
                file.close();
            }
            else {
                replyToCLient(ERROR, PSTR("CREATE FAILED"));
                return;
            }

        }
        if (path.lastIndexOf('/') > -1) {
            path = path.substring(0, path.lastIndexOf('/'));
        }
        replyToCLient(CUSTOM, path.c_str());
    }
    else {
        // Source specified: rename
        if (src == "/") {
            replyToCLient(BAD_REQUEST, PSTR("BAD SRC"));
            return;
        }
        if (!m_filesystem->exists(src)) {
            replyToCLient(BAD_REQUEST, PSTR("BSRC FILE NOT FOUND"));
            return;
        }

        DBG_OUTPUT_PORT.printf_P(PSTR("handleFileCreate: %s from %s\n"), path.c_str(), src.c_str());
        if (path.endsWith("/")) {
            path.remove(path.length() - 1);
        }
        if (src.endsWith("/")) {
            src.remove(src.length() - 1);
        }
        if (!m_filesystem->rename(src, path)) {
            replyToCLient(ERROR, PSTR("RENAME FAILED"));
            return;
        }
        replyOK();
    }
}

/*
    Handle a file deletion request
    Operation      | req.responseText
    ---------------+--------------------------------------------------------------
    Delete file    | parent of deleted file, or remaining ancestor
    Delete folder  | parent of deleted folder, or remaining ancestor
*/
void EspWebServer::handleFileDelete() {

    String path = server->arg(0);
    if (path.isEmpty() || path == "/") {
        replyToCLient(BAD_REQUEST, PSTR("BAD PATH"));
        return;
    }

    DBG_OUTPUT_PORT.printf_P(PSTR("handleFileDelete: %s\n"), path.c_str());
    if (!m_filesystem->exists(path)) {
        replyToCLient(NOT_FOUND, PSTR(FILE_NOT_FOUND));
        return;
    }
    //deleteRecursive(path);
    File root = m_filesystem->open(path, "r");
    // If it's a plain file, delete it
    if (!root.isDirectory()) {
        root.close();
        m_filesystem->remove(path);
        replyOK();
        return;
    }
}

/*
    This specific handler returns the edit.htm (or a gzipped version) from the /edit folder.
    If the file is not present but the flag INCLUDE_FALLBACK_INDEX_HTM has been set, falls back to the version
    embedded in the program code.
    Otherwise, fails with a 404 page with debug information
*/
void EspWebServer::handleGetEdit() {
    #ifdef INCLUDE_EDIT_HTM
    server->sendHeader(PSTR("Content-Encoding"), "gzip");
    server->send_P(200, "text/html", edit_htm_gz, sizeof(edit_htm_gz));
    #else
    replyToCLient(NOT_FOUND, PSTR("FILE_NOT_FOUND"));
    #endif
}

/*
    Return the FS type, status and size info
*/
void EspWebServer::handleStatus() {
    DBG_OUTPUT_PORT.println(PSTR("handleStatus"));
    FSInfo fs_info;
    String json;
    json.reserve(128);

    json = "{\"type\":\"File System\", \"isOk\":";
    if (m_fsOK) {
#ifdef ESP8266
        m_filesystem->info(fs_info);
#else
        updateFsInfo(&fs_info);
#endif
        json += PSTR("\"true\", \"totalBytes\":\"");
        json += fs_info.totalBytes;
        json += PSTR("\", \"usedBytes\":\"");
        json += fs_info.usedBytes;
        json += "\"";
    }
    else
        json += "\"false\"";
    json += PSTR(",\"unsupportedFiles\":\"\"}");
    server->send(200, "application/json", json);
}

#endif // INCLUDE_EDIT_HTM