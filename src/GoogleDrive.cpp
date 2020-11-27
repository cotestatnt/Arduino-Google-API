#include "GoogleDrive.h"


enum {WAIT_FILE, SAVE_ID, SAVE_NAME, SAVE_TYPE};

// external parser callback, just calls the internal one
void __DriveCallback(uint8_t filter, uint8_t level, const char *name, const char *value, void *cbObj)
{
	// call GoogleOAuth2 internal handler
	((GoogleDriveAPI *)cbObj)->parseDriveJson(filter, level, name, value);
}

// "GDrive specialized parser"
void GoogleDriveAPI::parseDriveJson(uint8_t filter, uint8_t level, const char *name, const char *value){
    static uint8_t _state = WAIT_FILE;
    static GoogleFile gFile;

    /*
    Serial.print("Level= ");
    Serial.println(level);
    Serial.print("Name = ");
    Serial.println(name);
    Serial.print("Value= ");
    Serial.println(value);
    Serial.println("--------------");
    */
    
    // populate filelist on searchFile() method (level2), or when new folder is created (level 0)
    if(level == 2 || level == 0) {

        if(strcmp(name, "kind") == 0 && strcmp(value, "drive#file") == 0){
            _state = SAVE_ID;
        }

        if(strcmp(name, "id") == 0 && _state == SAVE_ID){
            gFile.id = value;
            _state = SAVE_NAME;
        }

        if(strcmp(name, "name") == 0 && _state == SAVE_NAME){        
            gFile.name = value;
            _state = SAVE_TYPE;
            if(strcmp(_parser_drive.filter.keyword, value)==0){
                const char* id =  gFile.id.c_str();
                _parser_drive.setOutput(id, strlen(id));
            }
        }

        if(strcmp(name, "mimeType") == 0 && _state == SAVE_TYPE){
            if(strcmp(value, "application/vnd.google-apps.folder") == 0)
                gFile.isFolder = true;
            else
                gFile.isFolder = false;     
            if(_filelist != nullptr){       
                _filelist->addFile(gFile);                    
            }
            _state = WAIT_FILE;
        }

    }
}


GoogleDriveAPI::GoogleDriveAPI(fs::FS *fs) : GoogleOAuth2(fs)
{
    _filelist = nullptr;
    _parser_drive.setCallback(__DriveCallback, this);
}

GoogleDriveAPI::GoogleDriveAPI(fs::FS *fs, GoogleFilelist* list) : GoogleOAuth2(fs)
{
    _filelist = list;
    _parser_drive.setCallback(__DriveCallback, this);
}

GoogleDriveAPI::GoogleDriveAPI(fs::FS *fs, const char *configFile, GoogleFilelist* list) : GoogleOAuth2(fs, configFile)
{
    _filelist = list;
    _parser_drive.setCallback(__DriveCallback, this);
}


const char* GoogleDriveAPI::readClient(const char* funcName, const char* key)
{
    functionLog() ;

    // Init HTTP stream parser    
    _parser_drive.setSearchKey(funcName, key); 
	_parser_drive.reset();
    serialLog("\nLooking for value: ");  
    serialLogln(_parser_drive.filter.keyword); 
    serialLogln();

    // Skip headers
    while (ggclient.connected()) {
        static char old;
        char ch = ggclient.read();
        if (ch == '\n' && old == '\r') {
            break;
        }
        old = ch;
    }
    // get body content
    while (ggclient.available()) {
        char c = ggclient.read();
        _parser_drive.feed(c);
        serialLog((char)c);
        yield();
    }
    ggclient.stop();
    return nullptr;
}


// {{"title":"appData","mimeType":"application/vnd.google-apps.folder","parents":[{"id":"root"}]}
const char* GoogleDriveAPI::createAppFolder(const char *folderName)
{
    functionLog() ;
    char buffer[256];
    PString body(buffer, sizeof(buffer));
    body =  F("{\"client_id\":\"");
    body += readParam("client_id");
    body += F("\",\"name\":\"");
    body += folderName;
    body += F("\"parents\": \"root\",\"mimeType\": \"application/vnd.google-apps.folder\"}");    
    
    sendCommand("POST ", API_HOST, "/drive/v3/files", body, true);    
    serialLogln("\nHTTP Response:");
    readClient(__func__, folderName);
    const char* res = _parser_drive.getOutput();
    ggclient.stop();

    return res;
}

const char* GoogleDriveAPI::searchFile(const char *fileName)
{    
    functionLog() ;
    _filelist->clearList();
    sendCommand("GET ", API_HOST, "/drive/v3/files?q=trashed=false", "", true);
    readClient(__func__, fileName);
    const char* res = _parser_drive.getOutput();
    ggclient.stop();
    return res;
}

void GoogleDriveAPI::updateList(){
    functionLog() ;
    _filelist->clearList();
    sendCommand("GET ", API_HOST, "/drive/v3/files?q=trashed=false", "", true);
    readClient(__func__, "");
    ggclient.stop();
}

const char* GoogleDriveAPI::uploadFile(const char* path, const char* folderId, bool isUpdate)
{
    functionLog() ;
    #define BOUNDARY_UPLOAD "APP_UPLOAD_DATA"    
    File myFile = _filesystem->open(path, "r");
    if (!myFile) {
        Serial.printf("Failed to open file %s\n", path);
        return nullptr;
    }

    const char* fileid = folderId;   

    char * pch = strrchr(path,'/');
    size_t name_len = strlen(path) - (pch - path) - 1;
    char* filename = new char[name_len+1];
    filename[name_len] = '\0';
    strncpy(filename, pch+1, name_len);
        
    if(fileid != nullptr && isUpdate){
        serialLog("File already present, let's update it. ");
        serialLogln(fileid);
        sendMultipartFormData(path, filename, fileid, true);                
    } 
    else {
        serialLog("\nCreate new file\n");
        sendMultipartFormData(path, filename, folderId, false); 
    }
    
    readClient( __func__ , filename);
    const char* res = _parser_drive.getOutput();
    ggclient.stop();

    return res;   
}



bool GoogleDriveAPI::sendMultipartFormData(const char* path, const char* filename, const char* id, bool update)
{
    functionLog() ;
    #define BOUND_STR          "WebKitFormBoundary7MA4YWxkTrZu0gW"
    #define _BOUNDARY           "--" BOUND_STR
    #define END_BOUNDARY        "\r\n--" BOUND_STR "--\r\n"
    #define BLOCK_SIZE          512

    if (!ggclient.connected()) 
        doConnection(API_HOST);
      
    // token is still valid for less than 10s
    long r_second = expires_at_ms - (long) millis();
    serialLog(r_second/1000);
    serialLogln(PSTR(" seconds before token will expire."));
    
    if(r_second < 10000){
        serialLogln(PSTR("Call refresh token"));
        refreshToken();
    }
        
    // Check if file can be opened   
    File myFile = _filesystem->open(path, "r");
    if (!myFile) {
        Serial.print(PSTR("Failed to open file "));
        Serial.println(path);
        return false;
    }

    char buffer[512];
    PString tmpStr(buffer, sizeof(buffer));

    //aString start;
    if(update){
        tmpStr = PSTR("PATCH /upload/drive/v3/files/");
        tmpStr += id;
        tmpStr += PSTR("?uploadType=multipart");  
    }
    else 
        tmpStr = PSTR("POST /upload/drive/v3/files?uploadType=multipart");
    
    tmpStr += PSTR(" HTTP/1.1\r\nHost: ");
    tmpStr += API_HOST;
    tmpStr += PSTR("\r\nUser-Agent: ESP32\r\nConnection: keep-alive");

    serialLogln();
    serialLog(tmpStr);
    ggclient.print(tmpStr);

    tmpStr.begin();
    tmpStr = _BOUNDARY;
    tmpStr += PSTR("\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n");
    tmpStr += PSTR("{\"name\":\"");
    tmpStr += filename;
    // This is an upload in a specific folder
    if(!update && id != nullptr){
        tmpStr += PSTR("\",\"parents\":[\"");
        tmpStr += id;
        tmpStr += PSTR("\"]}\r\n\r\n");
    }
    else
        tmpStr += PSTR("\"}\r\n\r\n");    
    tmpStr += _BOUNDARY;
    tmpStr += PSTR("\r\nContent-Type: application/octet-stream\r\n\r\n");

    int len = myFile.size() + tmpStr.length() + 41 ; // sizeof END_BOUNDARY;
    char contentLen[5];
    itoa(len, contentLen, 10);

    ggclient.print("\r\nContent-Type: multipart/related; boundary=" BOUND_STR);
    ggclient.print("\r\nContent-Length: ");    
    ggclient.print(contentLen);
    ggclient.print("\r\nAuthorization: Bearer ");    
    ggclient.print(readParam("access_token"));
    ggclient.print("\r\n\r\n");

    serialLog(tmpStr);
    ggclient.print(tmpStr);

    uint8_t buff[BLOCK_SIZE];
    uint16_t count = 0; 
    while (myFile.available()) {    
        yield();    
        buff[count++] = myFile.read();      
        if (count == BLOCK_SIZE ) {
            serialLogln(PSTR("Sending binary data full buffer"));       
            ggclient.write((const uint8_t *)buff, BLOCK_SIZE);
            count = 0;
        }
    }
    if (count > 0) {        
        serialLogln(PSTR("Sending binary data remaining buffer")); 
        ggclient.write((const uint8_t *)buff, count);
    }

    serialLogln(END_BOUNDARY);
    ggclient.print(END_BOUNDARY);
    myFile.close();
    serialLogln();
    return true;
}


const char* GoogleDriveAPI::getFileName(int index)
{      
    return _filelist->getFileName(index);
}

const char* GoogleDriveAPI::getFileId(int index)
{
    return _filelist->getFileId(index);
}

const char* GoogleDriveAPI::getFileId(const char* name)
{
    return _filelist->getFileId(name);
}

unsigned int  GoogleDriveAPI::getNumFiles()
{
    return _filelist->size();
}
