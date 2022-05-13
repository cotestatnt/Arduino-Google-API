#include "GoogleDrive.h"

GoogleDriveAPI::GoogleDriveAPI(fs::FS& fs, Client& client, GoogleFilelist* list) : GoogleOAuth2(fs, client)
{
    m_filelist = list;
    // ID length can be 33 or 44 characters long
    m_appFolderId.reserve(MIN_ID_LEN + 11 + 1);
    m_lookingForId.reserve(MIN_ID_LEN + 11 + 1);
}

bool GoogleDriveAPI::parsePayload(String& payload, const int filter, const char* keyword)
{
    #if defined(ESP8266)
    DynamicJsonDocument doc(ESP.getMaxFreeBlockSize() - 512);
    #elif defined(ESP32)
    DynamicJsonDocument doc(ESP.getMaxAllocHeap() - 512);
    #else
    DynamicJsonDocument doc(payload.length() + 256);
    #endif
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        log_error("deserializeJson() failed\n");
        log_error("%s\n", err.c_str());
        return false;
    }

    switch (filter) {
        case SEARCH_ID: {
            if (doc["files"]) {
                JsonArray array = doc["files"].as<JsonArray>();
                for(JsonVariant file : array) {
                    if(file["name"].as<String>().equals(keyword)) {
                        log_debug("Found file \"%s\". ID: %s\n", keyword, file["id"].as<String>().c_str());
                        m_lookingForId = file["id"].as<String>();
                        return true;
                    }
                }
            }
            break;
        }

        case UPLOAD_ID: {
            const char* name =  doc["name"];
            if (strcmp(name, keyword) == 0) {
                m_lookingForId = doc["id"].as<String>();
                return true;
            }
        }

        case UPDATE_LIST: {
            if (doc["files"]) {
                JsonArray array = doc["files"].as<JsonArray>();
                for(JsonVariant file : array) {
                    if (!m_filelist->isInList(file["id"].as<String>().c_str())) {
                        const char* id = file["id"];
                        const char* name = file["name"];
                        bool isFolder = file["mimeType"].as<String>().equals("application/vnd.google-apps.folder");
                        log_debug("add file \"%s\" to Google Drive List", name);
                        m_filelist->addFile(name, id, isFolder);
                    }
                }
                log_debug("\n");
            }
            return true;
        }
    }

    return false;
}


bool GoogleDriveAPI::readDriveClient(const int filter, const char* keyword)
{
    String payload;
    readggClient(payload);
    return parsePayload(payload, filter, keyword);
}

const char* GoogleDriveAPI::searchFile(const char *fileName, const char* parentId)
{
    log_debug("Search file or folder %s\n", fileName);

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

    doConnection(API_HOST);
    sendCommand("GET ", API_HOST, cmd.c_str(), "", true);

    m_lookingForId = "";
    readDriveClient(SEARCH_ID, fileName);
    return m_lookingForId.c_str();
}

void GoogleDriveAPI::printFileList(){
    m_filelist->printList();
}

bool GoogleDriveAPI::updateFileList(){
    log_debug("Update the file list\n");
    if(!isAuthorized())
        return false;

    if(m_filelist == nullptr){
        Serial.println("error: the class was initialized without GoogleFilelist object");
        return false;
    }
    checkRefreshTime();

    m_filelist->clearList();
    sendCommand("GET ", API_HOST, "/drive/v3/files?orderBy=folder,name&q=trashed=false", "", true);
    return readDriveClient(UPDATE_LIST);
}


const char* GoogleDriveAPI::createFolder(const char *folderName, const char *parent, bool isName)
{
    log_debug("Create new folder %s", folderName);
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
    m_lookingForId = "";
    if(readDriveClient(UPLOAD_ID, folderName)){
        log_debug("add file \"%s\" to Google Drive List", folderName);
        m_filelist->addFile(folderName, m_lookingForId.c_str(), true);
    }
    return m_lookingForId.c_str();
}


// Set spreadsheet parents (with Drive API)
bool GoogleDriveAPI::setParentFolderId(const char* fileId, const char* parentId)
{
	// Set parent for the new created file
    String req = "/drive/v3/files/";
    req += fileId;
    req += "?addParents=";
    req += parentId;
    sendCommand("PATCH ", API_HOST, req.c_str(), "", true);
    // readClientSheet(SPREADSHEET_ID);
    log_debug("Set parent folder id %s\n", parentId);
    m_lookingForId = "";
    readDriveClient(UPLOAD_ID, parentId);
	return m_lookingForId.length() > 0;
}


const char* GoogleDriveAPI::uploadFile(const char* path, const char* id,  bool isUpdate)
{
    log_debug("Upload file %s", path);
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
        log_debug("File already present, let's update it. %s\n", id);
        sendMultipartFormData(path, filename, id, true);
    }
    else {
        log_debug("\nCreate new file\n");
        sendMultipartFormData(path, filename, id, false);
    }

    // GoogleFile_t gFile;
    // m_lookingForId = readClientDrive(UPLOAD_ID, &gFile);
    // return m_lookingForId.c_str();
    m_lookingForId = "";
    if (readDriveClient(UPLOAD_ID, filename)) {
        log_debug("add file \"%s\" to Google Drive List", filename);
        m_filelist->addFile(filename, m_lookingForId.c_str(), false);
    }

    return m_lookingForId.c_str();
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

    log_debug("%s", tmpStr.c_str());
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

    log_debug("%s", tmpStr.c_str());
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

    log_debug("%s\n", END_BOUNDARY);
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

