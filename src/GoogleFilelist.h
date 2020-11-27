
#include <Arduino.h>

struct GoogleFile{	
	String name;
	String id;
	bool isFolder;
    GoogleFile *nextFile = nullptr;
} ;


/*
struct GoogleFile{
    char* name;
    char* id;
    char* type;
    GoogleFile *nextFile = nullptr;
} ;
*/

class GoogleFilelist {

public:
    void clearList();
    unsigned int size();

    void addFile(const char* _name, const char* _id, bool _isFolder);
    void addFile(GoogleFile gFile);

    const char* getFileName(int index);    
    const char* getFileId(int index);
    const char* getFileId(const char* name);

    bool isFolder(int index);
    bool isFolder(const char* name);
    GoogleFilelist* getList();

    void printList();

private:

    unsigned int _filesCount;
	GoogleFile 	*_firstFile = nullptr;
	GoogleFile 	*_lastFile = nullptr;
};

