#ifndef GOOGLE_SHEET_API
#define GOOGLE_SHEET_API

#include "GoogleDrive.h"

#define API_DRIVE_HOST "www.googleapis.com"
#define API_SHEET_HOST "sheets.googleapis.com"

#define MAX_ROW_LEN 512

class GoogleSheetAPI : public GoogleDriveAPI
{

public:
    //GoogleSheetAPI(fs::FS &fs, Client &client, GoogleFilelist *list = nullptr);
    GoogleSheetAPI(GoogleOAuth2 *auth, GoogleFilelist *list = nullptr);
    ~GoogleSheetAPI(){ delete m_auth;};

    // Methods for handling single spreadsheet id
    inline const char *getSpreadsheetId() { return m_spreadsheetId.c_str(); }

    // Create a new spreadsheet and return the id
    const char *newSpreadsheet(const char *spreadsheetName, const char *parentId);
    inline const char *newSpreadsheet(const String &spreadsheetName, const String &parentId)
    {
        return newSpreadsheet(spreadsheetName.c_str(), parentId.c_str());
    }

    // Create a new sheet (in spreadsheet) and return the id (-1 on fail)
    int32_t newSheet(const char *sheetName, const char *spreadsheetId);

    // Check if sheetName is in spreadsheet and return sheet ID (0 is the first sheet created by default)
    int32_t getSheetId(const char *sheetName, const char *spreadsheetId);

    // Set new title for a specific sheet inside a spreadsheet
    bool setSheetTitle(const char *sheetTitle, const char *spreadsheetId, int32_t sheetId = 0);

    // Append row to the spreadsheet
    bool appendRowToSheet(const char *spreadsheetId, const char *range, const char *row);
    inline bool appendRowToSheet(const String &spreadsheetId, const String &range, const String &row)
    {
        return appendRowToSheet(spreadsheetId.c_str(), range.c_str(), row.c_str());
    }

    // Check if the spreadsheet is present in Google Drive
    const char* isSpreadSheet(const char *spreadsheetName, const char* parentName, bool createFolder = true);
    inline const char* isSpreadSheet(const String &spreadsheetName, const String &parentName, bool createFolder = true)
    {
        return isSpreadSheet(spreadsheetName.c_str(), parentName.c_str(), createFolder);
    }

    // Return the id of spreadsheet parent folder
    const char* getParentId() {
        return m_parentId.c_str();
    }

    bool updateSheetList(String &query);

private:
    String m_spreadsheetId;
    String m_parentId;

    // Class specialized parser
    bool readSheetClient(const int filter, const char *keyword = nullptr);
    bool parsePayload(const String &payload, const int filter, const char *gFile);
};
#endif
