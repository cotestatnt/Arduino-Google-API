#include "GoogleFilelist.h"


unsigned int GoogleFilelist::size(){
    return _filesCount;
}


void GoogleFilelist::clearList(){
    _firstFile = nullptr;
	_lastFile = nullptr;
}

void GoogleFilelist::addFile(const char* _name, const char* _id,  bool _isFolder){

    if(strlen(_id) < 30){
        return;     // Data not vali!    
    }
	
	for(GoogleFile *_file = _firstFile; _file != nullptr; _file = _file->nextFile){
        if( strcmp(_file->id, _id)==0 )
            return;		// File with provide id already present in list       	
	}
	
    GoogleFile *thisFile = new GoogleFile();
	if (_firstFile != nullptr) 
        _lastFile->nextFile = thisFile;		
	else
        _firstFile = thisFile;		

    _lastFile = thisFile;
	_filesCount++;
	thisFile->name = strdup(_name);
    thisFile->id =  strdup(_id);;	
    thisFile->isFolder = _isFolder;
}

void GoogleFilelist::addFile(GoogleFile gFile){
    //addFile(gFile.name.c_str(), gFile.id.c_str(), gFile.type.c_str());
    addFile(gFile.name, gFile.id, gFile.isFolder);
}



const char* GoogleFilelist::getFileName(int index){
    int counter = 0;
    for(GoogleFile *_file = _firstFile; _file != nullptr; _file = _file->nextFile){
        if(counter == index)
            return _file->name;
        counter++;		
	}
    return nullptr;
}

const char* GoogleFilelist::getFileId(int index){
    int counter = 0;
    for(GoogleFile *_file = _firstFile; _file != nullptr; _file = _file->nextFile){
        if(counter == index)
            return _file->id;
        counter++;		
	}
    return nullptr;
}

const char* GoogleFilelist::getFileId(const char* name){
    for(GoogleFile *_file = _firstFile; _file != nullptr; _file = _file->nextFile){
        if( strcmp(_file->name, name)==0  )	
            return _file->id;        	
	}
    return nullptr;
}


bool GoogleFilelist::isFolder(int index){    
    int counter = 0;
    for(GoogleFile *_file = _firstFile; _file != nullptr; _file = _file->nextFile){
        if (_file->isFolder && counter == index)
            return true;
        counter++;	
	}
    return false;
}


bool GoogleFilelist::isFolder(const char* name){        
    for(GoogleFile *_file = _firstFile; _file != nullptr; _file = _file->nextFile){
        if (_file->isFolder && strcmp(_file->name, name)==0)
            return true;        
	}
    return false;
}


GoogleFilelist* GoogleFilelist::getList(){
    return this;
}


void GoogleFilelist::printList(){
    for(GoogleFile *_file = _firstFile; _file != nullptr; _file = _file->nextFile){
        if (_file->isFolder)
            Serial.print("folder: ");
        else
            Serial.print("file:   "); 
        Serial.print(_file->name);
        Serial.print("\t ");
        Serial.println(_file->id);
	}
}