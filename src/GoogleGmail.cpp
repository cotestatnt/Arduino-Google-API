#include "GoogleGmail.h"
#include "Base64/Base64.h"



GoogleGmailAPI::GoogleGmailAPI(fs::FS *fs, GmailList * list) :  GoogleOAuth2(fs) {
    _mailList = list;
}


void GoogleGmailAPI::setMessageRead(const char* idEmail){
    functionLog() ;
    size_t len = strlen_P(PSTR("/gmail/v1/users/me/messages//modify")) + strlen(idEmail) + 1;
    char* cmd = new char[len];
    snprintf_P(cmd, len, PSTR("/gmail/v1/users/me/messages/%s/modify"), idEmail);

    sendCommand("POST ", GMAIL_HOST, cmd, "{\"removeLabelIds\": [\"UNREAD\"]}", true);
    readClient( __func__ , idEmail);
}



String GoogleGmailAPI::parseLine(String &line,  const char* _funcName, const char* _keyword){

    // Read email snippet only
    if(memcmp_P(_funcName, PSTR("mail_snippet"), strlen("mail_snippet")) == 0){
        String value = String('\0');
        value.reserve(200);
        value = getValue(line, "\"snippet\": \"" );
        if(value.length())  {
            return value;
        }
    }

    // Read email body only
    if(memcmp_P(_funcName, PSTR("mail_body"), strlen("mail_body") ) == 0){
        String value = getValue(line, "\"data\": \"" ).c_str();
        if(value.length()){
            size_t len = base64_dec_len(value.c_str(), value.length());
            char* decoded = new char[len+1];
            decoded[len] = '\0';
            base64_decode(decoded, value.c_str(), value.length());
            return (const char*) decoded;
        }
    }


    if(memcmp_P(_funcName, PSTR("sendEmail"), strlen("sendEmail")) == 0){
        // Send email response (the id and threadId of sent message)
        String value = String('\0');
        value.reserve(16);
        value = getValue(line, "\"id\": \"" );
        if(strlen(value.c_str()))
            return value.c_str();
    }

    if(memcmp_P(_funcName, PSTR("getMailList"), strlen("getMailList")) == 0){
        String value = String('\0');
        value.reserve(16);
        value = getValue(line, "\"id\": \"" );
        if(strlen(value.c_str()))
            _mailList->addMailId(value.c_str(), false);
    }

    if(memcmp_P(_funcName, PSTR("getMailData"), strlen("getMailData")) == 0){
        enum { NONE, DATE, FROM, SUBJECT};
        static int next = NONE;
        const char * id = _keyword;
        String value = String('\0');
        value.reserve(SUBJ_LEN);

        if (strstr_P(line.c_str(), PSTR("\"name\"")) && strstr(line.c_str(), "\"From\""))
            next = FROM;
        if (strstr_P(line.c_str(), PSTR("\"name\"")) && strstr(line.c_str(), "\"Date\""))
            next = DATE;
        if (strstr_P(line.c_str(), PSTR("\"name\"")) && strstr(line.c_str(), "\"Subject\""))
            next = SUBJECT;

        if( strstr_P(line.c_str(), PSTR("\"value\"")) && next == FROM){
            next = NONE;
            value = getValue(line, "\"value\": \"" );
            _mailList->setFrom(id, value.c_str() );
        }
        if( strstr_P(line.c_str(), PSTR("\"value\"")) && next == DATE){
            next = NONE;
            value = getValue(line, "\"value\": \"" );
            _mailList->setDate(id, value.c_str());
        }
        if( strstr_P(line.c_str(), PSTR("\"value\"")) && next == SUBJECT){
            next = NONE;
            value = getValue(line, "\"value\": \"" );
            _mailList->setSubject(id, value.c_str());
        }
    }

    return "";
}

String GoogleGmailAPI::readClient(const char* _funcName, const char* _key)
{
    functionLog() ;

    // Skip headers
    while (m_ggclient.connected()) {
        static char old;
        char ch = m_ggclient.read();
        if (ch == '\n' && old == '\r') {
            break;
        }
        old = ch;
    }

    while (m_ggclient.available()) {
        String line = m_ggclient.readStringUntil('\n');
        String res = parseLine( line, _funcName, _key);
        serialLogln(line);
        // value found in json response (skip all the remaining)
        if(res.length() > 0){
            while (m_ggclient.available()) {
                m_ggclient.read();
                yield();
            }
            m_ggclient.stop();
            return res;
        }
    }
    if(m_ggclient.connected())
        m_ggclient.stop();
    return "";
}



void GoogleGmailAPI::getMailData(const char* idEmail){
    functionLog() ;

    String cmd = F("/gmail/v1/users/me/messages/%s?format=metadata");
    cmd += idEmail;

    sendCommand("GET ", GMAIL_HOST, cmd.c_str(), "", true);
    readClient( "getMailData" , idEmail);
}

void GoogleGmailAPI::getMailList(const char* from, bool unread, uint32_t maxResults){
    functionLog() ;

    String uri = F("/gmail/v1/users/me/messages?maxResults=");
    uri += maxResults;

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

    sendCommand("GET ", GMAIL_HOST, uri.c_str(), "", true);
    readClient( "getMailList" , "");
}


const char* GoogleGmailAPI::sendEmail(const char* to, const char* subject, const char* message){
    functionLog() ;
    String tempsStr = F("To: ");
    tempsStr += to;
    tempsStr += F("\r\nSubject: ");
    tempsStr += subject;
    tempsStr += F("\r\n\r\n");
    tempsStr += message;

    //String encoded = base64::encode(tempsStr);
    //size_t enc_len = encoded.length()  + 32;

    int len = base64_enc_len(tempsStr.length());
    char* encoded = new char[len+1];
    encoded[len] = '\0';
    base64_encode( encoded, tempsStr.c_str(), tempsStr.length());

    tempsStr = F("{\"raw\": \"");
    tempsStr += encoded;
    tempsStr += F("\"}");

    sendCommand("POST ", GMAIL_HOST, "/gmail/v1/users/me/messages/send", tempsStr.c_str(), true);
    return readClient( "sendEmail" , "").c_str();
}


const char* GoogleGmailAPI::readSnippet(const char* idEmail){
    return readMail(idEmail, true);
}


const char* GoogleGmailAPI::readMail(const char* idEmail, bool snippet){
    functionLog() ;

    String cmd = F("/gmail/v1/users/me/messages/");
    cmd += idEmail;

    sendCommand("GET ", GMAIL_HOST, cmd.c_str(), "", true);
    const char * mail;
    if(snippet){
        mail = readClient( "mail_snippet" , idEmail).c_str();
    }
    else{
        mail = readClient( "mail_body" , idEmail).c_str();
    }
    return mail;
}

