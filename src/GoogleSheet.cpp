#include "GoogleSheet.h"

enum {APPEND_ROW, SHEET_ID};


GoogleSheetAPI::GoogleSheetAPI(fs::FS *fs) : GoogleDriveAPI(fs)
{
    m_sheetlist = nullptr;
}

GoogleSheetAPI::GoogleSheetAPI(fs::FS *fs, GoogleFilelist* list) : GoogleDriveAPI(fs)
{
    m_sheetlist = list;
}


String GoogleSheetAPI::parseLine(String &line, const int filter, const char* field = nullptr )
{
    String value;
    value.reserve(128);
    if(filter == APPEND_ROW ){
        value = getValue(line, "\"spreadsheetId\": \"" );
        if(value.length()) {
            return value;
        }
    }

    if(filter == SHEET_ID ){
        static String sheet_id;
        value = getValue(line, "\"sheetId\": " );
        if(value.length()) {
            sheet_id = value;
        }

        value = getValue(line, "\"title\": \"" );
        if(value.length()) {
            if(value.equals(field))
                return sheet_id;
        }
    }

    return "";
}


String GoogleSheetAPI::readClient(const int filter, const char* field = nullptr )
{
    functionLog() ;

    // Skip headers
    while (m_ggclient.connected()) {
        static char old;
        char ch = m_ggclient.read();
        if (ch == '\n' && old == '\r') {
            break;
        }
        old = ch;
    }

    // get body content
    String val;
    val.reserve(256);
    while (m_ggclient.available()) {
        String line = m_ggclient.readStringUntil('\n');
        serialLogln(line);

        // Parse line only when lenght is congruent with expected data (skip braces an blank lines)
        if (line.length() > 10)
            val = parseLine(line, filter, field);
        // value found in json response (skip all the remaining bytes)
        if(val.length()){
            while (m_ggclient.available()) {
                m_ggclient.read();
                yield();
            }
            m_ggclient.stop();
            return val;
        }

    }
    m_ggclient.stop();
    return  "";
}


bool GoogleSheetAPI::appendRowToSheet(const char* spreadsheetId, const char* range, const char* row ) {
    functionLog() ;

    String body = F("{\"values\": [");
    body += row;
    body += F("]}\n");

    String cmd =  F("/v4/spreadsheets/");
    cmd += spreadsheetId;
    cmd += F("/values/");
    cmd += range;
    cmd += F(":append?insertDataOption=OVERWRITE&valueInputOption=USER_ENTERED");

    sendCommand("POST ", API_SHEET_HOST, cmd.c_str(), body.c_str(), true);
    serialLogln("\nHTTP Response:");
    String res = readClient(APPEND_ROW);
    // If success res == sheet id (44 chars)
    if (res.length() > 40)
        return true;
    return false;
}



bool GoogleSheetAPI::newSheet(const char *sheetName, const char *spreadsheetId){
    // https://sheets.googleapis.com/v4/spreadsheets/1w56hn_rRMYa13RV677wK0_X-YpusnEoLbEgd9dwTIyc:batchUpdate
    functionLog() ;

    String body = F("{\"requests\":[{\"addSheet\":{\"properties\":{\"title\":\"");
    body += sheetName;
    body += F("\"}}}]}");

    String cmd =  F("/v4/spreadsheets/");
    cmd += spreadsheetId;
    cmd += F(":batchUpdate");

    sendCommand("POST ", API_SHEET_HOST, cmd.c_str(), body.c_str(), true);
    serialLogln("\nHTTP Response:");
    String res = readClient(APPEND_ROW);
    // If success res == sheet id (44 chars)
    if (res.length() > 40)
        return true;
    return false;
}


uint32_t  GoogleSheetAPI::hasSheet(const char *sheetName, const char *spreadsheetId){
    String cmd =  F("/v4/spreadsheets/");
    cmd += spreadsheetId;
    cmd += F("?&fields=sheets.properties");
    sendCommand("GET ", API_SHEET_HOST, cmd.c_str(), "", true);
    serialLogln("\nHTTP Response:");
    return readClient(SHEET_ID, sheetName).toInt();
}



// DRIVE methods

String GoogleSheetAPI::newSpreadsheet(const char *spreadsheetName, const char *parent, bool isName)
{
    functionLog() ;
    checkRefreshTime();
    String parentId;
    isName ? parentId = searchFile(parent) : parentId = parent;

    String body =  F("{\"client_id\":\"");
    body += readParam("client_id");
    body += F("\",\"name\":\"");
    body += spreadsheetName;
	body += F("\",\"mimeType\":\"application/vnd.google-apps.spreadsheet\",\"parents\":[\"");
    body += parentId.c_str();
    body += F("\"]}");

    sendCommand("POST ", API_DRIVE_HOST, "/drive/v3/files", body.c_str(), true);
    serialLogln("\nHTTP Response:");
    GoogleFile gFile;
    return GoogleDriveAPI::readClient(NEW_FILE, &gFile);
}


bool GoogleSheetAPI::updateSheetList(String& query){
    functionLog() ;

    if(m_sheetlist == nullptr){
        Serial.println("error: the class was initialized without GoogleFilelist object");
        return false;
    }
    checkRefreshTime();

    String cmd = F("/drive/v3/files?q=mimeType='application/vnd.google-apps.spreadsheet'");
    if(query.length()){
        query.replace(" ", "%20");
        cmd += query;
    }

    m_sheetlist->clearList();
    sendCommand("GET ", API_DRIVE_HOST, cmd.c_str(), "", true);
    GoogleFile gSheet;
    const char * res = GoogleDriveAPI::readClient(WAIT_FILE, &gSheet).c_str();
    if (strstr(res, "error") != NULL){
        return false;
    }
    return true;
}

