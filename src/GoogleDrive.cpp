#include "GoogleDrive.h"


GoogleDriveAPI::GoogleDriveAPI(GoogleOAuth2 *auth, GoogleFilelist *list) : m_auth(auth)
{
    if (list != nullptr)
        m_filelist = list;
    // ID length can be 33 or 44 characters long
    m_driveParentId.reserve(MIN_ID_LEN + 11 + 1);
    m_driveFileId.reserve(MIN_ID_LEN + 11 + 1);
}

bool GoogleDriveAPI::parsePayload(const String &payload, const int filter, const char *keyword)
{
    // functionLog();
#if defined(ESP8266)
    DynamicJsonDocument doc(ESP.getMaxFreeBlockSize() - 512);
#elif defined(ESP32)
    DynamicJsonDocument doc(ESP.getMaxAllocHeap() - 512);
#else
    DynamicJsonDocument doc(payload.length() + 256);
#endif
    DeserializationError err = deserializeJson(doc, payload);
    if (err)
    {
        log_error("deserializeJson() failed\n");
        log_error("%s\n", err.c_str());
        return false;
    }

    switch (filter)
    {
		case SEARCH_ID:
		{
			if (doc["files"])
			{
				JsonArray array = doc["files"].as<JsonArray>();
				for (JsonVariant file : array)
				{
					if (file["name"].as<String>().equals(keyword))
					{
						log_debug("Found file \"%s\". ID: %s\n", keyword, file["id"].as<String>().c_str());
						m_driveFileId = file["id"].as<String>();
						return true;
					}
				}
			}
			break;
		}

		case UPLOAD_ID:
		{
			const char *name = doc["name"];
			if (strcmp(name, keyword) == 0)
			{
				m_driveFileId = doc["id"].as<String>();
				return true;
			}
			break;
		}

		case UPDATE_LIST:
		{
			if (doc["files"])
			{
				JsonArray array = doc["files"].as<JsonArray>();
				for (JsonVariant file : array)
				{
					if (!m_filelist->isInList(file["id"].as<String>().c_str()))
					{
						const char *id = file["id"];
						const char *name = file["name"];
						bool isFolder = file["mimeType"].as<String>().equals("application/vnd.google-apps.folder");
						log_debug("add file \"%s\" to Google Drive List", name);
						m_filelist->addFile(name, id, isFolder);
					}
				}
				log_debug("\n");
				return true;
			}
			break;
		}
    }
    return false;
}

bool GoogleDriveAPI::readDriveClient(const int filter, const char *keyword,  bool payload_gzip)
{
    // functionLog();
    String payload;
    m_auth->readggClient(payload, true, payload_gzip);
    return parsePayload(payload, filter, keyword);
}

const char *GoogleDriveAPI::searchFile(const char *fileName, const char *parentId)
{
    log_debug("Search file or folder %s\n", fileName);

    if (!m_auth->isAuthorized())
        return nullptr;

    String cmd = F("/drive/v3/files?q=trashed=false%20and%20name%20=%20'");
    cmd += fileName;
    cmd += F("'");
    if (parentId != nullptr)
    {
        cmd += F("%20and%20'");
        cmd += parentId;
        cmd += F("'%20in%20parents");
    }

    m_auth->doConnection(API_HOST);
    m_auth->sendCommand("GET ", API_HOST, cmd.c_str(), "", true);

    m_driveFileId = "";
    readDriveClient(SEARCH_ID, fileName);
    return m_driveFileId.c_str();
}

void GoogleDriveAPI::printFileList() const
{
    m_filelist->printList();
}

bool GoogleDriveAPI::updateFileList()
{
    log_debug("Update the file list\n");
    if (!m_auth->isAuthorized())
        return false;

    if (m_filelist == nullptr)
    {
        Serial.println("error: the class was initialized without GoogleFilelist object");
        return false;
    }
    m_auth->checkRefreshTime();

    m_filelist->clearList();
    m_auth->sendCommand("GET ", API_HOST, "/drive/v3/files?orderBy=folder,name&q=trashed=false", "", true);
    return readDriveClient(UPDATE_LIST);
}

const char *GoogleDriveAPI::createFolder(const char *folderName, const char *parent, bool isName)
{
    log_debug("Create new folder %s", folderName);
    m_auth->checkRefreshTime();

    if (!m_auth->isAuthorized())
        return nullptr;

    String parentId;
    isName ? parentId = searchFile(parent) : parentId = parent;

    String body = F("{\"client_id\":\"");
    body += m_auth->readParam("client_id");
    body += F("\",\"name\":\"");
    body += folderName;
    body += F("\",\"mimeType\":\"application/vnd.google-apps.folder\",\"parents\":[\"");
    body += parentId.c_str();
    body += F("\"]}");

    m_auth->sendCommand("POST ", API_HOST, "/drive/v3/files", body.c_str(), true);
    m_driveFileId = "";
    if (readDriveClient(UPLOAD_ID, folderName))
    {
        log_debug("add folder \"%s\" to Google Drive List", folderName);
        m_filelist->addFile(folderName, m_driveFileId.c_str(), true);
    }
    return m_driveFileId.c_str();
}

// Set spreadsheet parents (with Drive API)
bool GoogleDriveAPI::setParentFolderId(const char *fileId, const char *parentId)
{
    // Set parent for the new created file
    String req = "/drive/v3/files/";
    req += fileId;
    req += "?addParents=";
    req += parentId;
    m_auth->sendCommand("PATCH ", API_HOST, req.c_str(), "", true);
    log_debug("Set parent folder id %s\n", parentId);
    m_driveFileId = "";
    readDriveClient(UPLOAD_ID, parentId);
    return m_driveFileId.length() > 0;
}

const char *GoogleDriveAPI::uploadFile(const char *path, const char *id, bool isUpdate)
{
    log_debug("Upload file %s", path);
    m_auth->checkRefreshTime();

    if (!m_auth->isAuthorized())
        return nullptr;

#define BOUNDARY_UPLOAD "APP_UPLOAD_DATA"
    File myFile = m_auth->m_filesystem->open(path, "r");
    if (!myFile || !m_auth->m_filesystem->exists(path))
    {
        Serial.printf("Failed to open file %s\n", path);
        return "";
    }

    char *pch = strrchr(path, '/');
    size_t name_len = strlen(path) - (pch - path) - 1;
    char *filename = new char[name_len + 1];
    filename[name_len] = '\0';
    strncpy(filename, pch + 1, name_len);

    if (id != nullptr && isUpdate)
    {
        log_debug("File already present, let's update it. %s\n", id);
        sendMultipartFormData(path, filename, id, true);
    }
    else
    {
        log_debug("\nCreate new file\n");
        sendMultipartFormData(path, filename, id, false);
    }

    m_driveFileId = "";
    if (readDriveClient(UPLOAD_ID, filename, false))
    {
        log_debug("add file \"%s\" to Google Drive List", filename);
        m_filelist->addFile(filename, m_driveFileId.c_str(), false);
    }

    return m_driveFileId.c_str();
}

const char *GoogleDriveAPI::uploadFile(const String &path, const String &id, bool isUpdate)
{
    return uploadFile(path.c_str(), id.c_str(), isUpdate);
}

bool GoogleDriveAPI::sendMultipartFormData(const char *path, const char *filename, const char *id, bool update)
{
    // functionLog();
#define BOUND_STR "WebKitFormBoundary7MA4YWxkTrZu0gW"
#define _BOUNDARY "--" BOUND_STR
#define END_BOUNDARY "\r\n--" BOUND_STR "--\r\n"
#define BLOCK_SIZE 1460

    if (!m_auth->m_ggclient->connected())
        m_auth->doConnection(API_HOST);

    // Check if file can be opened
    File myFile = m_auth->m_filesystem->open(path, "r");
    if (!myFile)
    {
        Serial.print(F("Failed to open file "));
        Serial.println(path);
        return false;
    }

    String tmpStr;
    tmpStr.reserve(512);
    if (update)
    {
        tmpStr = F("PATCH /upload/drive/v3/files/");
        tmpStr += id;
        tmpStr += F("?uploadType=multipart");
    }
    else
        tmpStr = F("POST /upload/drive/v3/files?uploadType=multipart");

    tmpStr += F(" HTTP/1.1\r\nHost: ");
    tmpStr += API_HOST;
    tmpStr += F("\r\nConnection: keep-alive\r\nAccept-Encoding: gzip");

    log_debug("%s", tmpStr.c_str());
    m_auth->m_ggclient->print(tmpStr);

    tmpStr = _BOUNDARY;
    tmpStr += F("\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n");
    tmpStr += F("{\"name\":\"");
    tmpStr += filename;
    // This is an upload in a specific folder
    if (!update && id != nullptr)
    {
        tmpStr += F("\",\"parents\":[\"");
        tmpStr += id;
        tmpStr += F("\"]}\r\n\r\n");
    }
    else
        tmpStr += F("\"}\r\n\r\n");
    tmpStr += _BOUNDARY;
    tmpStr += F("\r\nContent-Type: application/octet-stream\r\n\r\n");

    int len = myFile.size() + tmpStr.length() + 41; // sizeof END_BOUNDARY;
    char contentLen[5];
    itoa(len, contentLen, 10);
    m_auth->m_ggclient->print(F("\r\nAccept-Encoding: gzip"));
    m_auth->m_ggclient->print(F("\r\nContent-Type: multipart/related; boundary=" BOUND_STR));
    m_auth->m_ggclient->print(F("\r\nContent-Length: "));
    m_auth->m_ggclient->print(contentLen);
    m_auth->m_ggclient->print(F("\r\nAuthorization: Bearer "));
    m_auth->m_ggclient->print(m_auth->readParam("access_token"));
    m_auth->m_ggclient->print(F("\r\n\r\n"));

    log_debug("%s", tmpStr.c_str());
    m_auth->m_ggclient->print(tmpStr);

    uint8_t buff[BLOCK_SIZE];
    uint16_t count = 0;
    while (myFile.available())
    {
        yield();
        buff[count++] = (uint8_t)myFile.read();
        if (count == BLOCK_SIZE)
        {
            count = 0;
            m_auth->m_ggclient->write(buff, BLOCK_SIZE);
        }
    }
    if (count > 0)
    {
        m_auth->m_ggclient->write(buff, count);
    }

    m_auth->m_ggclient->print(END_BOUNDARY);
    myFile.close();

    log_debug("%s\n", END_BOUNDARY);
    return true;
}

const char *GoogleDriveAPI::getFileName(int index) const
{
    return (char *)m_filelist->getFileName(index);
}

const char *GoogleDriveAPI::getFileId(int index) const
{
    return (char *)m_filelist->getFileId(index);
}

const char *GoogleDriveAPI::getFileId(const char *name) const
{
    return (char *)m_filelist->getFileId(name);
}

unsigned int GoogleDriveAPI::getNumFiles() const
{
    return m_filelist->size();
}
