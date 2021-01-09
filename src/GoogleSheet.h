
#ifndef GOOGLESHEETAPI
#define GOOGLESHEETAPI

#include "GoogleDrive.h"

#define API_DRIVE_HOST "www.googleapis.com"
#define API_SHEET_HOST "sheets.googleapis.com"

#define MAX_ROW_LEN 256


struct SheetProperties{	
	char* name;
	char* id;
	bool isFolder;
    GoogleFile *nextFile = nullptr;
} ;

class GoogleSheetAPI : public GoogleDriveAPI
{
    
public:

    GoogleSheetAPI (fs::FS *fs );
    GoogleSheetAPI (fs::FS *fs, GoogleFilelist* list);
    GoogleSheetAPI (fs::FS *fs, const char *configSheet, GoogleFilelist* list);

    // Methods for handling the spreadsheet list
    inline void     printSheetList()                { m_sheetlist->printList(); }
    inline char*    getListSheetName(int index)     { return (char*) m_sheetlist->getFileName(index); }
    inline char*    getListSheetId(int index)       { return (char*) m_sheetlist->getFileId(index); }
    inline char*    getListSheetId(const char* name){ return (char*) m_sheetlist->getFileId(name); }
    inline uint16_t getListNumSheets()              { return m_sheetlist->size(); }
    bool            updateSheetList(String& query);

    // Methods for handling single spreadsheet id
    inline void     setSheetId(const char* id)  { m_spreadsheet_id = strdup(id); }
    inline void     setSheetId(String id)       { setSheetId(id.c_str()); }
    inline char*    getSheetId()                { return m_spreadsheet_id; }

    // // Create a new spreadsheet and return the id
    String          newSpreadsheet(const char *spreadsheetName, const char *parent, bool isName = false);
    inline String   newSpreadsheet(String& spreadsheetName, String& parent, bool isName = false) {  
                                            return newSpreadsheet(spreadsheetName.c_str(), parent.c_str(), isName);  }
    
    // Create a new sheet (in spreadsheet) and return the id
    bool      newSheet(const char *sheetName, const char *spreadsheetId);
    // Check if sheetName is in spreadsheet and return sheet ID (unsigned long)
    uint32_t  hasSheet(const char *sheetName, const char *spreadsheetId);
    
    // Append row to the spreadsheet
    bool appendRowToSheet(const char* spreadsheetId, const char* range, const char* row) ;

private:
    char* m_spreadsheet_id = (char*)"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";   // 44 random chars
    String readClient(const int expected, const char* field );
    String parseLine(String &line, const int expected, const char* field);
    GoogleFilelist * m_sheetlist = nullptr;
};

#endif
