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
    ~GoogleFilelist();

    void clearList();
    unsigned int size() const;

    void addFile(const char* name, const char* id, bool isFolder);
    void addFile(const String& name, const String& id_t, bool isFolder);
    void addFile(const GoogleFile_t& gFile);

    const char* getFileName(int index) const;
    const char* getFileId(int index) const;
    const char* getFileId(const char* name) const;
    const char* getFileId(const String& name) const;

    bool isFolder(int index) const;
    bool isFolder(const char* name) const;
    bool isFolder(String& name) const;
    bool isInList(const char* id) const;

    void printList() const;

private:

    unsigned int m_filesCount = 0;
	GoogleFile_t *m_firstFile = nullptr;
	GoogleFile_t *m_lastFile = nullptr;
};

#endif