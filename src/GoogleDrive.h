
#ifndef GOOGLEO_DRIVE_API
#define GOOGLEO_DRIVE_API

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
    inline void setAppFolderId(const char* folderId) {
        m_appFolderId = String(folderId);
    }
    inline void setAppFolderId(String folderId) {
        m_appFolderId = folderId;
    }
    inline const char* getAppFolderId() {
        return m_appFolderId.c_str();
    }

    // return the google ID  for files or folder
    const char*  createFolder(const char *folderName, const char *parent, bool isName = false);
    inline const char*  createFolder(String& folderName, const char *parent, bool isName = false) {
        return createFolder(folderName.c_str(), parent, isName);
    }
    inline const char*  createFolder(String& folderName, String& parent, bool isName = false) {
        return createFolder(folderName.c_str(), parent.c_str(), isName);
    }

    bool setParentFolderId(const char* fileId, const char* parentId);

    const char*  searchFile(const char *fileName,  const char* parentId = nullptr);
    inline const char* searchFile(String& fileName,  String& parentId) {
        return searchFile(fileName.c_str(), parentId.c_str());
    }
    inline const char* searchFile(String& fileName ) {
        return searchFile(fileName.c_str(), nullptr);
    }

    bool    updateFileList();
    void    printFileList();

    // Upload or update file
    const char* uploadFile( const char* path, const char* id, bool isUpdate = true);
    const char* uploadFile( String &path, String &id, bool isUpdate = true);

protected:
    enum {WAIT_FILE, SAVE_ID, SAVE_NAME, SAVE_TYPE, SEARCH_ID, UPLOAD_ID, NEW_FILE, UPDATE_LIST};

    GoogleFilelist* m_filelist;
    String m_appFolderId;
    String m_lookingForId;
    bool sendMultipartFormData(const char* path, const char* filename, const char* id, bool update = false);

    String parseLine(String &line, const int filter, GoogleFile_t* gFile );
    String uploadFileName;

    // const char* readClientDrive(const int filter, GoogleFile_t* gFile = nullptr );


    // Class specialized parser
    bool readDriveClient(const int filter,  const char* keyword = nullptr );
    bool parsePayload(String& payload, const int filter, const char* gFile);
};

#endif
