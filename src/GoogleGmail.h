
#ifndef GOOGLEOGMAILAPI
#define GOOGLEOGMAILAPI

#include <Arduino.h>
#include "GoogleOAuth2.h"

#define GMAIL_HOST "gmail.googleapis.com"

class GmailList ;

class GoogleGmailAPI : public GoogleOAuth2
{
    friend void __MailCallback(uint8_t filter, uint8_t level, const char *name, const char *value, void *cbObj);
   

public:
    GoogleGmailAPI(fs::FS *fs, GmailList * list);
    GoogleGmailAPI(fs::FS *fs, const char *configFile, GmailList * list);

    const char* sendEmail(const char * to, const char * subject, const char * message);

    const char* sendEmail(String& to, String& subject, String& message){
        sendEmail(to.c_str(), subject.c_str(), message.c_str());
        return _parser_mail.getOutput(); 
    }
    const char * getMailList(const char * from = nullptr, bool unread = true, uint32_t maxResults =100);    
    const char * readMail(const char * idEmail, bool snippet = false);
    const char * readSnippet(const char * idEmail);
    void getMailData(const char* idEmail);
    void setMessageRead(const char* idEmail);
protected:
    JSONStreamingParser _parser_mail;
    void parseMailJson(uint8_t filter, uint8_t level, const char *name, const char *value);
    const char* readClient(const char* funcName, const char* key);    

    GmailList  *_mailList;
};






class GmailList {

    struct mailItem{	
        char* id;
        char* from;
        char* subject;
        char* date;
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
        mailItem *thisItem = new mailItem();
        if (_firstItem != nullptr) 
            _lastItem->nextItem = thisItem;		
        else
            _firstItem = thisItem;		
        _lastItem = thisItem;
        _mailCount++;
        thisItem->read = _read;
        thisItem->id = (char*)realloc(thisItem->id, sizeof(char *) * (strlen(_id)+ 1));
        strcpy(thisItem->id, _id);
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
        return "not found";
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
        functionLog() ;
        // Address is like: name surname <myemail@gmail.com> but with unicode
        size_t len = strlen(value) - 8;  // length of string - (length of "u003c" + "u003e" replaced with < >);
        mailItem * item = getItem(id);
        if(item != nullptr)
            if( !strcmp(item->id, id) ){  
                item->from = (char*)realloc( item->from, sizeof(char *) * (len+ 1));
                item->from[len] = '\0';

                size_t pos = strstr(value, "u003c") - value;
                strncpy(item->from, value, pos);
                item->from[pos] = '<';
                strncpy(item->from + pos + 1, value + pos + 5, strlen(value) -9 - pos);
                item->from[len -1] = '>';                
            }        
    }
    void setSubject (const char* id, const char * value){
        size_t len = strlen(value);
        mailItem * item = getItem(id);
        if(item != nullptr)
            if( !strcmp(item->id, id) ){     
                item->subject = (char*)realloc( item->subject, sizeof(char *) * (len+ 1));
                strcpy( item->subject, value);
            }    
    }
    void setDate (const char* id, const char * value){
        size_t len = strlen(value);
        mailItem * item = getItem(id);
        if(item != nullptr)
            if( !strcmp(item->id, id) ){     
                item->date = (char*)realloc( item->date, sizeof(char *) * (len+ 1));
                strcpy( item->date, value);
            }      
    }
};



#endif
