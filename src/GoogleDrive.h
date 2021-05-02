
#ifndef GOOGLEODRIVEAPI
#define GOOGLEODRIVEAPI

#include "GoogleFilelist.h"
#include "GoogleOAuth2.h"

#define API_HOST "www.googleapis.com"

class GoogleDriveAPI : public GoogleOAuth2
{

public:
    GoogleDriveAPI (fs::FS& fs, Client& client, GoogleFilelist* list = nullptr);

    unsigned int    getNumFiles();
    const char*     getFileName(int index);
    const char*     getFileId(int index);
    const char*     getFileId(const char* name);

    // Methods to store and retrieve app filder id (if files are organized by folder)
    inline void  setAppFolderId(const char* folderId) { m_appFolderId = strdup(folderId); }
    inline void  setAppFolderId(String folderId) { setAppFolderId(folderId.c_str()); }
    inline char* getAppFolderId(){ return m_appFolderId; }

    // return the google ID  for files or folder
    const char*  createFolder(const char *folderName, const char *parent, bool isName = false);
    const char*  searchFile(const char *fileName,  const char* parentId = nullptr);

    // inline String  searchFile(String& fileName,  String& parentId) { return searchFile(fileName.c_str(), parentId.c_str()); }
    // inline String  searchFile(String& fileName ) { return searchFile(fileName.c_str(), nullptr); }

    bool    updateFileList();
    void    printFileList();

    // Upload or update file
    const char* uploadFile( const char* path, const char* id, bool isUpdate = true);
    const char* uploadFile( String &path, String &id, bool isUpdate = true);

protected:
    enum {WAIT_FILE, SAVE_ID, SAVE_NAME, SAVE_TYPE, SEARCH_ID, UPLOAD_ID, NEW_FILE};

    GoogleFilelist* m_filelist;
    // init var with 50 chars length (max file id len should be 44)
    char* m_appFolderId = (char*) "**************************************************";
    char* m_lookingForId = (char*) "**************************************************";
    bool sendMultipartFormData(const char* path, const char* filename, const char* id, bool update = false);
    String readClient(const int expected, GoogleFile* gFile = nullptr );
    String parseLine(String &line, const int filter, GoogleFile* gFile );

    String uploadFileName;
};

#endif
