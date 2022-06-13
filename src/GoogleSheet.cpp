#include "GoogleSheet.h"

enum
{
    SPREADSHEET_ID,
    PARENT_ID,
    APPEND_ROW,
    SHEET_ID,
    ADD_SHEET_ID
};


GoogleSheetAPI::GoogleSheetAPI(GoogleOAuth2 *auth, GoogleFilelist *list) : GoogleDriveAPI(auth, list)
{
    m_spreadsheetId.reserve(MIN_ID_LEN + 11 + 1);
}

bool GoogleSheetAPI::parsePayload(const String &payload, const int filter, const char *keyword)
{
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
    case SPREADSHEET_ID:
    case APPEND_ROW:
    {
        if (doc["spreadsheetId"])
        {
            m_spreadsheetId = doc["spreadsheetId"].as<String>();
            return true;
        }
        break;
    }

    case PARENT_ID:
    {
        if (doc["parents"][0]["id"].as<String>().equals(keyword))
        {
            m_spreadsheetId = doc["id"].as<String>();
            return true;
        }
        break;
    }

    case SHEET_ID:
    {
        if (doc["sheets"])
        {
            JsonArray array = doc["sheets"].as<JsonArray>();
            for (JsonVariant sheet : array)
            {
                if (sheet["properties"]["title"].as<String>().equals(keyword))
                {
                    m_spreadsheetId = sheet["properties"]["sheetId"].as<String>();
                    return true;
                }
            }
        }
        break;
    }

    case ADD_SHEET_ID:
    {
        if (doc["replies"][0]["addSheet"])
        {
            m_spreadsheetId = doc["replies"][0]["addSheet"]["properties"]["sheetId"].as<String>();
            return true;
        }
        break;
    }
    }
    return false;
}

bool GoogleSheetAPI::readSheetClient(const int filter, const char *keyword)
{
    String payload;
    m_auth->readggClient(payload);
    return parsePayload(payload, filter, keyword);
}

bool GoogleSheetAPI::appendRowToSheet(const char *spreadsheetId, const char *range, const char *row)
{
    String body = F("{\"values\": [");
    body += row;
    body += F("]}\n");

    String cmd = F("/v4/spreadsheets/");
    cmd += spreadsheetId;
    cmd += F("/values/");
    cmd += range;
    cmd += F(":append?insertDataOption=OVERWRITE&valueInputOption=USER_ENTERED");

    m_auth->sendCommand("POST ", API_SHEET_HOST, cmd.c_str(), body.c_str(), true);
    m_spreadsheetId = "";
    readSheetClient(SPREADSHEET_ID);
    return (m_spreadsheetId.length() > 0);
}

int32_t GoogleSheetAPI::newSheet(const char *sheetName, const char *spreadsheetId)
{
    // https://sheets.googleapis.com/v4/spreadsheets/1w56hn_rRMYa13RV677wK0_X-YpusnEoLbEgd9dwTIyc:batchUpdate

    int32_t sheetId = -1;
    String body = F("{\"requests\":[{\"addSheet\":{\"properties\":{\"title\":\"");
    body += sheetName;
    body += F("\"}}}]}");

    String cmd = F("/v4/spreadsheets/");
    cmd += spreadsheetId;
    cmd += F(":batchUpdate");

    m_auth->sendCommand("POST ", API_SHEET_HOST, cmd.c_str(), body.c_str(), true);
    m_spreadsheetId = "";
    if (readSheetClient(ADD_SHEET_ID, sheetName))
    {
        log_debug("New Sheet created, id: %ld\n", m_spreadsheetId.toInt());
        sheetId = m_spreadsheetId.toInt();
    }
    return sheetId;
}

int32_t GoogleSheetAPI::getSheetId(const char *sheetName, const char *spreadsheetId)
{
    int32_t sheetId = -1;
    String cmd = F("/v4/spreadsheets/");
    cmd += spreadsheetId;
    cmd += F("?&fields=sheets.properties");
    m_auth->sendCommand("GET ", API_SHEET_HOST, cmd.c_str(), "", true);
    m_spreadsheetId = "";

    if (readSheetClient(SHEET_ID, sheetName))
    {
        log_debug("Sheet id: %ld\n", m_spreadsheetId.toInt());
        sheetId = m_spreadsheetId.toInt();
    }
    return sheetId;
}

bool GoogleSheetAPI::setSheetTitle(const char *sheetTitle, const char *spreadsheetId, int32_t sheetId)
{
    String cmd = F("/v4/spreadsheets/");
    cmd += spreadsheetId;
    cmd += F(":batchUpdate");

    String body = F("{\"requests\": [{\"updateSheetProperties\": {\"properties\": {\"sheetId\": ");
    body += sheetId;
    body += F(",\"title\":\"");
    body += sheetTitle;
    body += F("\"},\"fields\": \"title\"}}]}");

    m_auth->sendCommand("POST ", API_SHEET_HOST, cmd.c_str(), body.c_str(), true);
    m_spreadsheetId = "";
    readSheetClient(SPREADSHEET_ID);
    return (m_spreadsheetId.length() > 0);
}

// Create spreadsheet (with Drive API) with a single sheet (default name)
const char *GoogleSheetAPI::newSpreadsheet(const char *spreadsheetName, const char *parentId)
{
    log_debug("Create new spreadsheet %s", spreadsheetName);
    m_auth->checkRefreshTime();

    if (!m_auth->isAuthorized())
        return nullptr;

    String body = F("{\"client_id\":\"");
    body += m_auth->readParam("client_id");
    body += F("\",\"name\":\"");
    body += spreadsheetName;
    body += F("\",\"mimeType\":\"application/vnd.google-apps.spreadsheet\",\"parents\":[\"");
    body += parentId;
    body += F("\"]}");

    m_auth->sendCommand("POST ", API_HOST, "/drive/v3/files", body.c_str(), true);
    m_driveFileId = "";
    if (GoogleDriveAPI::readDriveClient(GoogleDriveAPI::UPLOAD_ID, spreadsheetName))
    {
        log_debug("Spreadsheet id: %s\n", m_driveFileId.c_str());
    }
    return m_driveFileId.c_str();
}

bool GoogleSheetAPI::updateSheetList(String &query)
{
    if (m_filelist == nullptr)
    {
        Serial.println(F("error: the class was initialized without GoogleFilelist object"));
        return false;
    }
    m_auth->checkRefreshTime();

    String cmd = F("/drive/v3/files?q=mimeType='application/vnd.google-apps.spreadsheet'");
    if (query.length())
    {
        query.replace(" ", "%20");
        cmd += query;
    }

    m_filelist->clearList();
    m_auth->sendCommand("GET ", API_DRIVE_HOST, cmd.c_str(), "", true);
    return GoogleDriveAPI::readDriveClient(GoogleDriveAPI::UPDATE_LIST);
}


// Check if the spreadsheet exist in provided folder (with Drive API)
const char* GoogleSheetAPI::isSpreadSheet(const char *spreadsheetName, const char *parentName, bool doCreateFolder)
{
    m_parentId = searchFile(parentName);
    if(m_parentId.length() == 0 && doCreateFolder) {
        Serial.println("Parent not found");
        m_parentId = createFolder(parentName, "root");
        log_debug("App folder id: %s\n", m_parentId.c_str());
    }

    m_driveFileId = searchFile(spreadsheetName, m_parentId.c_str());
    return m_driveFileId.c_str();
}