
#ifndef GOOGLEODRIVEAPI
#define GOOGLEODRIVEAPI

#include "GoogleFilelist.h"
#include "GoogleOAuth2.h"

#define API_HOST "www.googleapis.com"

class GoogleDriveAPI : public GoogleOAuth2
{

public:



    GoogleDriveAPI (fs::FS *fs );
    GoogleDriveAPI (fs::FS *fs, GoogleFilelist* list);
    GoogleDriveAPI (fs::FS *fs, const char *configFile, GoogleFilelist* list);

    unsigned int    getNumFiles();
    const char*     getFileName(int index);
    const char*     getFileId(int index);
    const char*     getFileId(const char* name);

    // Methods to store and retrieve app filder id (if files are organized by folder)    
    inline void  setAppFolderId(const char* folderId) { m_appFolderId = strdup(folderId); }
    inline void  setAppFolderId(String folderId) { setAppFolderId(folderId.c_str()); }
    inline char* getAppFolderId(){ return m_appFolderId; }

    // return the google ID  for files or folder    
    char*   createFolder(const char *folderName, const char *parent, bool isName = false);
    String  searchFile(const char *fileName,  const char* parentId = nullptr);
    inline String  searchFile(String& fileName,  String& parentId) { return searchFile(fileName.c_str(), parentId.c_str()); }
    inline String  searchFile(String& fileName ) { return searchFile(fileName.c_str(), nullptr); }

    bool    updateFileList();
    void    printFileList();

    // Upload or update file
    char*   uploadFile(const char* path, const char* folderId, bool isUpdate = true);
    char*   uploadFile(String &path, String &folderId, bool isUpdate = true); 

protected:
    enum {WAIT_FILE, SAVE_ID, SAVE_NAME, SAVE_TYPE, SEARCH_ID, UPLOAD_ID, NEW_FILE};

    GoogleFilelist* m_filelist;
    char* m_appFolderId = (char*)"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"; // 33 random chars
    bool sendMultipartFormData(const char* path, const char* filename, const char* id, bool update = false);
    String readClient(const int expected, GoogleFile* gFile = nullptr );
    String parseLine(String &line, const int filter, GoogleFile* gFile );

    String uploadFileName;
};

#endif
