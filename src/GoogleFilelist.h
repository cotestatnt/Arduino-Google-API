#include "GoogleOAuth2.h"

#ifndef GOOGLE_FILE_LIST
#define GOOGLE_FILE_LIST

struct GoogleFile_t{
	String name;
	String id;
	bool isFolder;
    GoogleFile_t *nextFile = nullptr;
};

class GoogleFilelist {

public:
    void clearList();
    unsigned int size();

    void addFile(const char* name, const char* id, bool isFolder);
    void addFile(String& name, String& id_t, bool isFolder);
    void addFile(const GoogleFile_t& gFile);

    const char* getFileName(int index);
    const char* getFileId(int index);
    const char* getFileId(const char* name);
    const char* getFileId(String& name);

    bool isFolder(int index);
    bool isFolder(const char* name);
    bool isFolder(String& name);
    bool isInList(const char* id);
    GoogleFilelist* getList();

    void printList();

private:

    unsigned int m_filesCount = 0;
	GoogleFile_t *m_firstFile = nullptr;
	GoogleFile_t *m_lastFile = nullptr;
};

#endif