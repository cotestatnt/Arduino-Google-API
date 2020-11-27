#include "GoogleGmail.h"
#include "include/base64/base64.h"

// external parser callback, just calls the internal one
void __MailCallback(uint8_t filter, uint8_t level, const char *name, const char *value, void *cbObj)
{
	// call GoogleOAuth2 internal handler
	((GoogleGmailAPI *)cbObj)->parseMailJson(filter, level, name, value);
}

// "GMail specialized parser"
void GoogleGmailAPI::parseMailJson(uint8_t filter, uint8_t level, const char *name, const char *value){
        
    static bool done = false;     

    // mail list
    if(level == 2){        
        if( !strcmp(name, "id") && !strcmp(_parser_mail.getCallFunction(), "getMailList")){   
            _mailList->addMailId(value, false);
            done = false; 
        }
    }

    // mail metadata
    if(level == 3  && !strcmp(_parser_mail.getCallFunction(), "getMailData")){
        enum { NONE, DATE, FROM, SUBJECT};
        static int next = NONE;
        const char * id = _parser_mail.getCallKeyword();

        if (!strcmp(name, "name") && !strcmp(value, "From"))
            next = FROM;
        if (!strcmp(name, "name") && !strcmp(value, "Date"))
            next = DATE;
        if (!strcmp(name, "name") && !strcmp(value, "Subject"))
            next = SUBJECT;

        if( !strcmp(name, "value") && next == FROM){   
            next = NONE;
            _mailList->setFrom(id, value);
        }
        if( !strcmp(name, "value")  && next == DATE){    
            next = NONE;
            _mailList->setDate(id, value);
        }
        if( !strcmp(name, "value") && next == SUBJECT){    
            next = NONE;
            _mailList->setSubject(id, value);
        }
        
    }

    if( level == 0 ){

        // Read email snippet only
        if(!strcmp(_parser_mail.getCallKeyword(), "mail_snippet")){            
            if( !done && !strcmp(name, "snippet") ){   
                done = true;                        
                _parser_mail.setOutput(value, strlen(value));
            }
        }

        // Last value of json response (to reset done variable)
        if( !strcmp(_parser_mail.filter.keyword, "internalDate") ){
            done = false;
        }

        // Send email response (the id and threadId of sent message)
        if( !strcmp(name, "id") && !strcmp(_parser_mail.getCallFunction(), "sendEmail")){   
            _parser_mail.setOutput(value, strlen(value));            
        }
    }

    if(level == 4 && !done ) {              
        // Read email body and decode
        if( !strcmp(name, "data") && !strcmp(_parser_mail.getCallKeyword(), "mail_body")){                                   
            done = true;
            size_t dec_size = b64_decoded_size(value);
            char* decoded = new char[dec_size+1];
            // decode base 64 body message
            if(base64_decode(value, decoded, dec_size)){
                _parser_mail.setOutput(decoded, dec_size);
            }
                                   
        }        
    }   
    
}

GoogleGmailAPI::GoogleGmailAPI(fs::FS *fs, GmailList * list) :  GoogleOAuth2(fs) {
    _mailList = list;
    _parser_mail.setCallback(__MailCallback, this);

}

GoogleGmailAPI::GoogleGmailAPI(fs::FS *fs, const char *configFile, GmailList * list) :  GoogleOAuth2(fs, configFile){  
    _mailList = list;
    _parser_mail.setCallback(__MailCallback, this);
}

void GoogleGmailAPI::setMessageRead(const char* idEmail){
    functionLog() ;
    size_t len = strlen_P(PSTR("/gmail/v1/users/me/messages//modify")) + strlen(idEmail) + 1;
    char* cmd = new char[len];
    snprintf_P(cmd, len, PSTR("/gmail/v1/users/me/messages/%s/modify"), idEmail);

    sendCommand("POST ", GMAIL_HOST, cmd, "{\"removeLabelIds\": [\"UNREAD\"]}", true);
    readClient( __func__ , idEmail);
}


const char* GoogleGmailAPI::readClient(const char* funcName, const char* key)
{
    functionLog() ;

    // Init HTTP stream parser
    _parser_mail.setSearchKey(funcName, key);
	_parser_mail.reset();
    
    serialLog("\nLooking for value: ");    
    serialLogln(_parser_mail.filter.keyword); 
    serialLogln();

    // Skip headers
    while (ggclient.connected()) {
        static char old;
        char ch = ggclient.read();
        if (ch == '\n' && old == '\r') {
            break;
        }
        old = ch;
    }
    // get body content
    while (ggclient.available()) { 
        char ch = ggclient.read();
        if(_parser_mail.getOutput() == NULL){
            _parser_mail.feed(ch);
            delayMicroseconds(10);
        }
        serialLog((char)ch);
        yield();
    }    
    ggclient.stop();
    return nullptr;
}



void GoogleGmailAPI::getMailData(const char* idEmail){
    functionLog() ;

    size_t len = strlen_P(PSTR("/gmail/v1/users/me/messages/?format=metadata")) + strlen(idEmail) + 1;
    char* cmd = new char[len];
    snprintf_P(cmd, len, PSTR("/gmail/v1/users/me/messages/%s?format=metadata"), idEmail);

    sendCommand("GET ", GMAIL_HOST, cmd, "", true);
    readClient( __func__ , idEmail);
}

const char* GoogleGmailAPI::getMailList(const char* from, bool unread, uint32_t maxResults){
    functionLog() ;

    char buffer[128] ;
    PString uri(buffer, sizeof(buffer));

    //uri = ("/gmail/v1/users/me/messages?maxResults=100");
    uri.format_P(PSTR("/gmail/v1/users/me/messages?maxResults=%d"), maxResults);

    if(from != nullptr || unread){
        uri += PSTR("&q=");
        if(from != nullptr && unread){
            uri += PSTR("from:");
            uri += from;
            uri += PSTR("%20is:unread");
        }
        else if(from != nullptr){
            uri += PSTR("from:");
            uri += from;
        }
        else if (unread)
            uri += PSTR("is:unread");
    }

    sendCommand("GET ", GMAIL_HOST, uri, "", true);
    readClient( __func__ , "messages");
    return _parser_mail.getOutput();  
}


const char* GoogleGmailAPI::sendEmail(const char* to, const char* subject, const char* message){
    functionLog() ;
    size_t clear_len = strlen(to) + strlen(subject) + strlen(message) + 32;
    char* base64Buf = (char*)malloc( sizeof(char *) * clear_len);    
    snprintf_P(base64Buf, clear_len, PSTR("To: %s\t\nSubject: %s\r\n\r\n%s"), to, subject, message);
    
    const char * base64enc = base64_encode((uint8_t *)base64Buf, strlen(base64Buf));
    size_t enc_len = strlen(base64enc) + 32;

    char * body = (char*) malloc( sizeof(char*) * enc_len);
    snprintf_P(body, enc_len, PSTR("{\"raw\": \"%s\"}"), base64enc);

    sendCommand("POST ", GMAIL_HOST, "/gmail/v1/users/me/messages/send", body, true);
    readClient( __func__ , "id");
    return _parser_mail.getOutput();  
}


const char* GoogleGmailAPI::readSnippet(const char* idEmail){
    return readMail(idEmail, true);
}


const char* GoogleGmailAPI::readMail(const char* idEmail, bool snippet){
    functionLog() ;
    size_t len = strlen_P(PSTR("/gmail/v1/users/me/messages/")) + strlen(idEmail) + 1;
    char* cmd = new char[len];
    snprintf_P(cmd, len, PSTR("/gmail/v1/users/me/messages/%s"), idEmail);

    sendCommand("GET ", GMAIL_HOST, cmd, "", true);
    if(snippet)
        readClient( __func__ , "mail_snippet");
    else
        readClient( __func__ , "mail_body");    

    return _parser_mail.getOutput();
}

