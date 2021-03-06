#include "GoogleDrive.h"

GoogleDriveAPI::GoogleDriveAPI(fs::FS& fs, Client& client, GoogleFilelist* list) : GoogleOAuth2(fs, client)
{
    m_filelist = list;
}

String GoogleDriveAPI::parseLine(String &line, const int filter, GoogleFile* gFile )
{
    static uint8_t t_state = WAIT_FILE;
    String value;
    value.reserve(128);

    value = getValue(line, "\"error\": \"" );
    if(value.length()) {
        String err = "error: ";
        err += value;
        t_state = WAIT_FILE;
        return err;
    }

    if(filter == SEARCH_ID || filter == NEW_FILE){
        value = getValue(line, "\"id\": \"" );
        if(value.length()) {
            return value;
        }
    }

    switch(t_state ){
        case WAIT_FILE:
            value = getValue(line, "\"kind\": \"" );
            if(value.length()) {
                if(value.indexOf("drive#file") > -1)
                    t_state = SAVE_ID;

                if(value.indexOf("drive#fileList") > -1)
                    t_state = WAIT_FILE;
            }
            break;

        case SAVE_ID:
            value = getValue(line, "\"id\": \"" );
            if(value.length()) {
                if(gFile != nullptr)
                    gFile->id = strdup(value.c_str());
                t_state = SAVE_NAME;
            }
            break;

        case SAVE_NAME:
            value = getValue(line, "\"name\": \"" );
            if(value.length()) {
                if(gFile != nullptr)
                    gFile->name = strdup(value.c_str());
                t_state = SAVE_TYPE;
            }
            break;

        case SAVE_TYPE:
            value = getValue(line, "\"mimeType\": \"" );
            if(value.length()) {
                if(gFile != nullptr){
                    if(value.indexOf(F("application/vnd.google-apps.folder")) > -1)
                        gFile->isFolder = true;
                    else
                        gFile->isFolder = false;
                    if(m_filelist != nullptr){
                        m_filelist->addFile(*gFile);
                    }

                    if(filter == UPLOAD_ID) {
                        t_state = WAIT_FILE;
                        return gFile->id;
                    }
                }
                t_state = WAIT_FILE;
            }
            break;
        default:
            break;
    }
    return "";
}

const char* GoogleDriveAPI::readClient(const int filter, GoogleFile* gFile)
{
    functionLog() ;
    String val = "";
    // Skip headers
    while (m_ggclient->connected()) {
        String line = m_ggclient->readStringUntil('\n');
        if (line == "\r") {
            serialLogln(line);
            break;
        }
    }
    // get body content
    while (m_ggclient->available()) {
        String line = m_ggclient->readStringUntil('\n');
        serialLogln(line);

        // Parse line only when lenght is congruent with expected data (skip braces an blank lines)
        if (line.length() > 10)
            val = parseLine(line, filter, gFile);

        // value found in json response (skip all the remaining bytes)
        if(val.length()){
            while (m_ggclient->available()) {
                m_ggclient->read();
                yield();
            }
        }
    }
    //m_ggclient->stop();

    if(val.length() == 0)
        memset(m_lookingForId, '\0', sizeof(m_lookingForId));
    strcpy(m_lookingForId, val.c_str());
    return (const char*)m_lookingForId;
}



const char* GoogleDriveAPI::searchFile(const char *fileName, const char* parentId)
{
    functionLog() ;
    checkRefreshTime();

    if(!isAuthorized())
        return nullptr;

    String cmd = F("/drive/v3/files?q=trashed=false%20and%20name%20=%20'");
    cmd += fileName;
    cmd += F("'");
    if(parentId != nullptr){
        cmd += F("%20and%20'");
        cmd += parentId;
        cmd += F("'%20in%20parents");
    }

    sendCommand("GET ", API_HOST, cmd.c_str(), "", true);
    //return  readClient(SEARCH_ID);

    const char* resultId = readClient(SEARCH_ID);
    strcpy(m_lookingForId, resultId);
    return (const char*)m_lookingForId;
}

void GoogleDriveAPI::printFileList(){
    m_filelist->printList();
}

bool GoogleDriveAPI::updateFileList(){
    functionLog() ;

    if(!isAuthorized())
        return false;

    if(m_filelist == nullptr){
        Serial.println("error: the class was initialized without GoogleFilelist object");
        return false;
    }
    checkRefreshTime();

    m_filelist->clearList();
    sendCommand("GET ", API_HOST, "/drive/v3/files?orderBy=folder,name&q=trashed=false", "", true);
    GoogleFile gFile;
    const char * res = readClient(WAIT_FILE, &gFile);
    if (strstr(res, "error") != NULL){
        return false;
    }
    return true;
}



// {{"title":"appData","mimeType":"application/vnd.google-apps.folder","parents":[{"id":"root"}]}
const char* GoogleDriveAPI::createFolder(const char *folderName, const char *parent, bool isName)
{
    functionLog() ;
    checkRefreshTime();

    if(!isAuthorized())
        return nullptr;

    String parentId;
    isName ? parentId = searchFile(parent) : parentId = parent;

    String body =  F("{\"client_id\":\"");
    body += readParam("client_id");
    body += F("\",\"name\":\"");
    body += folderName;
	body += F("\",\"mimeType\":\"application/vnd.google-apps.folder\",\"parents\":[\"");
    body += parentId.c_str();
    body += F("\"]}");

    sendCommand("POST ", API_HOST, "/drive/v3/files", body.c_str(), true);
    serialLogln("\nHTTP Response:");

    GoogleFile gFile;
    const char* resultId = readClient(UPLOAD_ID, &gFile);
    strcpy(m_lookingForId, resultId);
    return (const char*)m_lookingForId;
}




const char* GoogleDriveAPI::uploadFile(const char* path, const char* id,  bool isUpdate)
{
    functionLog() ;
    checkRefreshTime();

    if(!isAuthorized())
        return nullptr;

    #define BOUNDARY_UPLOAD "APP_UPLOAD_DATA"
    File myFile = m_filesystem->open(path, "r");
    if (!myFile || !m_filesystem->exists(path) ) {
        Serial.printf("Failed to open file %s\n", path);
        return "";
    }

    char * pch = strrchr(path,'/');
    size_t name_len = strlen(path) - (pch - path) - 1;
    char* filename = new char[name_len+1];
    filename[name_len] = '\0';
    strncpy(filename, pch+1, name_len);

    if(id != nullptr && isUpdate){
        serialLog("File already present, let's update it. ");
        serialLogln(id);
        sendMultipartFormData(path, filename, id, true);
    }
    else {
        serialLog("\nCreate new file\n");
        sendMultipartFormData(path, filename, id, false);
    }

    GoogleFile gFile;
    const char* resultId = readClient(UPLOAD_ID, &gFile);
    strcpy(m_lookingForId, resultId);

    return (const char*)m_lookingForId;
}

const char* GoogleDriveAPI::uploadFile(String &path, String &id, bool isUpdate)
 {
    return uploadFile(path.c_str(), id.c_str(), isUpdate);
 }


bool GoogleDriveAPI::sendMultipartFormData(const char* path, const char* filename, const char* id, bool update)
{
    functionLog() ;
    #define BOUND_STR          "WebKitFormBoundary7MA4YWxkTrZu0gW"
    #define _BOUNDARY           "--" BOUND_STR
    #define END_BOUNDARY        "\r\n--" BOUND_STR "--\r\n"
	#define BLOCK_SIZE          1460

    if (!m_ggclient->connected())
        doConnection(API_HOST);

    // Check if file can be opened
    File myFile = m_filesystem->open(path, "r");
    if (!myFile) {
        Serial.print(PSTR("Failed to open file "));
        Serial.println(path);
        return false;
    }

    String tmpStr;
    tmpStr.reserve(512);
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
    m_ggclient->print(tmpStr);

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

    m_ggclient->print("\r\nContent-Type: multipart/related; boundary=" BOUND_STR);
    m_ggclient->print("\r\nContent-Length: ");
    m_ggclient->print(contentLen);
    m_ggclient->print("\r\nAuthorization: Bearer ");
    m_ggclient->print(readParam("access_token"));
    m_ggclient->print("\r\n\r\n");

    serialLog(tmpStr);
    m_ggclient->print(tmpStr);
	
    uint8_t buff[BLOCK_SIZE];
	uint16_t count = 0;
    while (myFile.available()) {
		yield();
		buff[count++] = (uint8_t)myFile.read();        
        if (count == BLOCK_SIZE ) {            
			count = 0;
            m_ggclient->write(buff, BLOCK_SIZE);
        }
	}
    if (count > 0) {
		m_ggclient->write(buff, count);
	}

    m_ggclient->print(END_BOUNDARY);
    myFile.close();

    serialLogln(END_BOUNDARY);
    serialLogln();
    return true;
}



const char* GoogleDriveAPI::getFileName(int index)
{
    return (char*) m_filelist->getFileName(index);
}

const char* GoogleDriveAPI::getFileId(int index)
{
    return (char*) m_filelist->getFileId(index);
}

const char* GoogleDriveAPI::getFileId(const char* name)
{
    return (char*) m_filelist->getFileId(name);
}

unsigned int  GoogleDriveAPI::getNumFiles()
{
    return m_filelist->size();
}

