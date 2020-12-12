
#ifndef GOOGLEOGMAILAPI
#define GOOGLEOGMAILAPI

#include <Arduino.h>
#include "GoogleOAuth2.h"
#include <time.h>  

#define GMAIL_HOST "gmail.googleapis.com"

class GmailList ;

class GoogleGmailAPI : public GoogleOAuth2
{

public:
    GoogleGmailAPI(fs::FS *fs, GmailList * list);
    GoogleGmailAPI(fs::FS *fs, const char *configFile, GmailList * list);

    const char* sendEmail(const char * to, const char * subject, const char * message);
    const char* sendEmail(String& to, String& subject, String& message){        
        return sendEmail(to.c_str(), subject.c_str(), message.c_str());
    }
    void getMailList(const char * from = nullptr, bool unread = true, uint32_t maxResults =100);    
    const char * readMail(const char * idEmail, bool snippet = false);
    const char * readSnippet(const char * idEmail);
    void getMailData(const char* idEmail);
    void setMessageRead(const char* idEmail);
protected:
    String readClient(const char* _funcName, const char* _key);    
    String parseLine(String& line,  const char* _funcName, const char* _keyword);

    GmailList  *_mailList;
};


// Define some limits for metadata infos
#define ID_LEN 16
#define FROM_LEN 64
#define SUBJ_LEN 128
#define DATE_LEN 37

class GmailList {

    struct mailItem{	
        char id[ID_LEN+1];
        char from[FROM_LEN+1];
        char subject[SUBJ_LEN+1];
        char date[DATE_LEN+1];
        bool read = false;
        mailItem *nextItem = nullptr;
    } ;

    friend class GoogleGmailAPI; 

public:
    void clear(){
        _firstItem = nullptr;
	    _lastItem = nullptr;
    }
    unsigned int size(){
        return _mailCount;
    }

    void addMailId( const char* _id, bool _read){
        for(mailItem *_item = _firstItem; _item != nullptr; _item = _item->nextItem){
            if( strcmp(_item->id, _id)==0 )
                return;		//  id already present in list       	
        }
        mailItem *thisItem = new mailItem();
        if (_firstItem != nullptr) 
            _lastItem->nextItem = thisItem;		
        else
            _firstItem = thisItem;		
        _lastItem = thisItem;
        _mailCount++;
        thisItem->read = _read;
        memcpy(thisItem->id, _id, ID_LEN);
        thisItem->id[ID_LEN] = '\0';
    }

    const char* getMailId(int index){
        int counter = 0;
        for(mailItem *_item = _firstItem; _item != nullptr; _item = _item->nextItem){
            if(counter == index)
                return _item->id;
            counter++;		
        }
        return "not found";
    }
    const char* getFrom (const char* id){ 
        mailItem * item = getItem(id);
        if(item != nullptr){
            return item->from;
        }
        return "not found";
    }
    const char* getDate (const char* id){
        mailItem * item = getItem(id);
        if(item != nullptr)
            return item->date;      
        return 0;
    }
    const char* getSubject (const char* id){ 
        mailItem * item = getItem(id);
        if(item != nullptr)
            return item->subject; 
        return "not found";
    }
    void printList(){        
        for(mailItem *_item = _firstItem; _item != nullptr; _item = _item->nextItem){
            Serial.print("\tid: ");
            Serial.println(_item->id);
        }
    }

protected:
    unsigned int _mailCount;
	mailItem *_firstItem = nullptr;
	mailItem *_lastItem = nullptr;

    mailItem * getItem(const char* id){
        for(mailItem *_item = _firstItem; _item != nullptr; _item = _item->nextItem)
            if( !strcmp(_item->id, id) )
                return _item; 
        return nullptr;
    }

    void setFrom (const char* id, const char * value){ 
        // Address is like: name surname <myemail@gmail.com> but with unicode
        size_t len = strlen(value) - 10;  // length of string - (length of "\u003c" + "\u003e" replaced with < >);
        mailItem * item = getItem(id);
        if(item != nullptr)
            if( !strcmp(item->id, id) ){  
                memset(item->from, '\0', FROM_LEN );
                size_t pos = strstr(value, "u003c") - value;
                strncpy(item->from, value, pos);
                item->from[pos-1] = '<';
                strncpy(item->from + pos , value + pos + 5, strlen(value) - 10 - pos);
                item->from[len -1] = '>';                
            }        
    }
    void setSubject (const char* id, const char * value){
        size_t len = strlen(value);
        mailItem * item = getItem(id);
        if(item != nullptr)
            if( !strcmp(item->id, id) ){     
                memset(item->subject, '\0', FROM_LEN );
                strncpy( item->subject, value, len);
            }    
    }
    void setDate (const char* id, const char * value){
        mailItem * item = getItem(id);
        if(item != nullptr)
            if( !strcmp(item->id, id) ){     
                memset(item->date, '\0', DATE_LEN );
                strncpy( item->date, value, DATE_LEN);
            }      
    }
};



#endif
