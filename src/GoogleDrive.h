
#ifndef GOOGLEODRIVEAPI
#define GOOGLEODRIVEAPI

#include "GoogleFilelist.h"
#include "GoogleOAuth2.h"

#define API_HOST "www.googleapis.com"


class GoogleDriveAPI : public GoogleOAuth2
{
    friend void __DriveCallback(uint8_t filter, uint8_t level, const char *name, const char *value, void *cbObj);
    
public:

    GoogleDriveAPI (fs::FS *fs );
    GoogleDriveAPI (fs::FS *fs, GoogleFilelist* list);
    GoogleDriveAPI (fs::FS *fs, const char *configFile, GoogleFilelist* list);

    unsigned int    getNumFiles();
    const char*     getFileName(int index);
    const char*     getFileId(int index);
    const char*     getFileId(const char* name);

    // return the google ID  for files or folder
    const char*     createAppFolder(const char *folderName);
    const char*     searchFile(const char *fileName);
    void            updateList();

    // Upload or update file
    const char*     uploadFile(const char* path, const char* folderId, bool isUpdate = true);

protected:
    JSONStreamingParser _parser_drive;
    void parseDriveJson(uint8_t filter, uint8_t level, const char *name, const char *value);
   
private:
    const char*     appFolderId;
    GoogleFilelist  *_filelist;
    bool sendMultipartFormData(const char* path, const char* filename, const char* id, bool update = false);
    const char* readClient(const char* funcName, const char* key);
};

#endif
