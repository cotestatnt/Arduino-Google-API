#include "GoogleFilelist.h"

unsigned int GoogleFilelist::size(){
    return m_filesCount;
}

void GoogleFilelist::clearList(){
    m_firstFile = nullptr;
	m_lastFile = nullptr;
}

void GoogleFilelist::addFile(const char* name, const char* id,  bool isFolder){    

	for(GoogleFile_t *file = m_firstFile; file != nullptr; file = file->nextFile){
        if( file->id.equals(id) ) {
            log_error("ID %s already present in list\n", id);    
            return;		// File with provided id already present in list
        }
	}

    GoogleFile_t *thisFile = new GoogleFile_t();
	if (m_firstFile != nullptr)
        m_lastFile->nextFile = thisFile;
	else
        m_firstFile = thisFile;

    m_lastFile = thisFile;
	m_filesCount++;
	thisFile->name = name;
    thisFile->id =  id;
    thisFile->isFolder = isFolder;
}

void GoogleFilelist::addFile(const GoogleFile_t& gFile){
    addFile(gFile.name.c_str(), gFile.id.c_str(), gFile.isFolder);
}

void GoogleFilelist::addFile(String& name, String& id, bool isFolder){
    addFile(name.c_str(), id.c_str(), isFolder);
}


const char* GoogleFilelist::getFileName(int index){
    int counter = 0;
    for(GoogleFile_t *file = m_firstFile; file != nullptr; file = file->nextFile){
        if(counter == index)
            return file->name.c_str();
        counter++;
	}
    return nullptr;
}

const char* GoogleFilelist::getFileId(int index){
    int counter = 0;
     for(GoogleFile_t *file = m_firstFile; file != nullptr; file = file->nextFile){
        if(counter == index)
            return file->id.c_str();
        counter++;
	}
    return nullptr;
}

const char* GoogleFilelist::getFileId(const char* name){
    for(GoogleFile_t *file = m_firstFile; file != nullptr; file = file->nextFile){
        if( file->name.equals(name))
            return file->id.c_str();
	}
    return nullptr;
}

const char* GoogleFilelist::getFileId(String& name){
    return getFileId(name.c_str());
}


bool GoogleFilelist::isInList(const char* id){
    for(GoogleFile_t *file = m_firstFile; file != nullptr; file = file->nextFile){
        if( file->id.equals(id))
            return true;
	}
    return false;
}

bool GoogleFilelist::isFolder(int index){
    int counter = 0;
    for(GoogleFile_t *file = m_firstFile; file != nullptr; file = file->nextFile){
        if (file->isFolder && counter == index)
            return true;
        counter++;
	}
    return false;
}


bool GoogleFilelist::isFolder(const char* name){
    for(GoogleFile_t *file = m_firstFile; file != nullptr; file = file->nextFile){
        if (file->isFolder && file->name.equals(name))
            return true;
	}
    return false;
}

bool GoogleFilelist::isFolder(String& name) {
    return isFolder(name.c_str());
}


GoogleFilelist* GoogleFilelist::getList(){
    return this;
}


void GoogleFilelist::printList(){
    for(GoogleFile_t *file = m_firstFile; file != nullptr; file = file->nextFile){
        if (file->isFolder)
            Serial.print("folder:\t");
        else
            Serial.print("file:\t");
        Serial.print(file->name);
        Serial.print("\t ");
        Serial.println(file->id);
	}
}