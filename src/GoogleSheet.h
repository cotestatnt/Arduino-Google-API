#ifndef GOOGLE_SHEET_API
#define GOOGLE_SHEET_API

#include "GoogleDrive.h"

#define API_DRIVE_HOST "www.googleapis.com"
#define API_SHEET_HOST "sheets.googleapis.com"

#define MAX_ROW_LEN 512

class GoogleSheetAPI : public GoogleDriveAPI
{

public:
    GoogleSheetAPI (fs::FS& fs, Client& client, GoogleFilelist* list = nullptr);

    // Methods for handling the spreadsheet list
    inline void     printSheetList()                { m_sheetlist->printList(); }
    inline char*    getListSheetName(int index)     { return (char*) m_sheetlist->getFileName(index); }
    inline char*    getListSheetId(int index)       { return (char*) m_sheetlist->getFileId(index); }
    inline char*    getListSheetId(const char* name){ return (char*) m_sheetlist->getFileId(name); }
    inline uint16_t getListNumSheets()              { return m_sheetlist->size(); }
    bool            updateSheetList(String& query);

    // Methods for handling single spreadsheet id
    inline const char*  getSpreadsheetId()              { return m_spreadsheetId.c_str(); }

    // Create a new spreadsheet and return the id
    const char*  newSpreadsheet(const char *spreadsheetName,  const char *parentId);
    inline const char*  newSpreadsheet(String& spreadsheetName, String& parentId) {
        return newSpreadsheet(spreadsheetName.c_str(), parentId.c_str() );
    }

    // Create a new sheet (in spreadsheet) and return the id (-1 on fail)
    int32_t  newSheet(const char *sheetName, const char *spreadsheetId);

    // Check if sheetName is in spreadsheet and return sheet ID (0 is the first sheet created by default)
    int32_t  getSheetId(const char *sheetName, const char *spreadsheetId);

    // Set new title for a specific sheet inside a spreadsheet
    bool setSheetTitle(const char *sheetTitle, const char *spreadsheetId, int32_t sheetId = 0);

    // Append row to the spreadsheet
    bool appendRowToSheet(const char* spreadsheetId, const char* range, const char* row) ;
    inline bool appendRowToSheet(String& spreadsheetId, String& range, String& row) {
        return appendRowToSheet(spreadsheetId.c_str(), range.c_str(), row.c_str()) ;
    }

private:
    String m_spreadsheetId;
    String m_lookingForId;
    String readClientSheet(const int expected, const char* field );
    String parseLine(String &line, const int expected, const char* field);
    GoogleFilelist * m_sheetlist = nullptr;

    // Class specialized parser
    bool readSheetClient(const int filter,  const char* keyword = nullptr );
    bool parsePayload(String& payload, const int filter, const char* gFile);
};
#endif
