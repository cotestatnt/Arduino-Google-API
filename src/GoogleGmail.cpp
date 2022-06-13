#include "GoogleGmail.h"
#include "Base64.h"




GoogleGmailAPI::GoogleGmailAPI(GoogleOAuth2 *auth, GmailList *list)
{
    if (list != nullptr)
        m_mailList = list;
    m_auth = auth;
}


String GoogleGmailAPI::parsePayload(const String& payload, const int filter, const char* keyword)
{
    #if defined(ESP8266)
    DynamicJsonDocument doc(ESP.getMaxFreeBlockSize() - 512);
    #elif defined(ESP32)
    DynamicJsonDocument doc(ESP.getMaxAllocHeap() - 512);
    #else
    DynamicJsonDocument doc(payload.length() + 256);
    #endif
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        log_error("deserializeJson() failed\n");
        log_error("%s\n", err.c_str());
        return "";
    }

    String result = "";
    switch (filter) {
        case MAIL_ID:
            if (doc["id"])
                result = doc["id"].as<String>();
            break;

        case MAIL_LIST:
            if (doc["messages"]) {
                result = doc["resultSizeEstimate"].as<String>();
                JsonArray array = doc["messages"].as<JsonArray>();
                for(JsonVariant message : array) {
                    m_mailList->addMailId(message["id"].as<String>().c_str(), false);
                }
            }
            break;

        case READ_SNIPPET:
            if (doc["snippet"]) {
                result = doc["snippet"].as<String>();
                GmailList::mailItem* mail = m_mailList->getItem(keyword);
                JsonArray array = doc["payload"]["headers"].as<JsonArray>();
                for(JsonVariant header : array) {
                    if (header["name"].as<String>().equals("Date"))
                        mail->date = header["value"].as<String>();
                    if (header["name"].as<String>().equals("Subject"))
                        mail->subject = header["value"].as<String>();
                    if (header["name"].as<String>().equals("From"))
                        mail->from = header["value"].as<String>();
                }
            }
            break;

        case READ_EMAIL:
            if (doc["payload"]) {
                GmailList::mailItem* mail = m_mailList->getItem(keyword);
                JsonArray array = doc["payload"]["headers"].as<JsonArray>();
                for(JsonVariant header : array) {
                    if (header["name"].as<String>().equals("Date"))
                        mail->date = header["value"].as<String>();
                    if (header["name"].as<String>().equals("Subject"))
                        mail->subject = header["value"].as<String>();
                    if (header["name"].as<String>().equals("From"))
                        mail->from = header["value"].as<String>();
                }
                result = Base64::decode(doc["payload"]["parts"][0]["body"]["data"].as<String>());
            }
            break;
    }
    return result;
}


String GoogleGmailAPI::readGMailClient(const int filter, const char* keyword)
{
    String payload;
    m_auth->readggClient(payload);
    return parsePayload(payload, filter, keyword);
}


bool GoogleGmailAPI::setMessageRead(const char* idEmail){
    size_t len = strlen_P(PSTR("/gmail/v1/users/me/messages//modify")) + strlen(idEmail) + 1;
    char* cmd = new char[len];
    snprintf_P(cmd, len, PSTR("/gmail/v1/users/me/messages/%s/modify"), idEmail);

    m_auth->sendCommand("POST ", GMAIL_HOST, cmd, "{\"removeLabelIds\": [\"UNREAD\"]}", true);
	return (readGMailClient(MAIL_ID, idEmail) != "");
}


int GoogleGmailAPI::getMailData(const char* idEmail){
    String cmd = PSTR("/gmail/v1/users/me/messages/%s?format=metadata");
    cmd += idEmail;

    m_auth->sendCommand("GET ", GMAIL_HOST, cmd.c_str(), "", true);
    return readGMailClient(READ_SNIPPET, idEmail ).length();
}

int GoogleGmailAPI::getMailList(const char* from, bool unread, uint32_t maxResults){
    String uri = F("/gmail/v1/users/me/messages?maxResults=");
    uri += maxResults;

    if(from != nullptr || unread){
        uri += F("&q=");
        if(from != nullptr && unread){
            uri += F("from:");
            uri += from;
            uri += F("%20is:unread");
        }
        else if(from != nullptr){
            uri += F("from:");
            uri += from;
        }
        else if (unread)
            uri += F("is:unread");
    }

    m_auth->sendCommand("GET ", GMAIL_HOST, uri.c_str(), "", true);
    return readGMailClient(MAIL_LIST).toInt();
}


String GoogleGmailAPI::sendEmail(const char* to, const char* subject, const char* message){

    String tempsStr = F("To: ");
    tempsStr += to;
    tempsStr += F("\r\nSubject: ");
    tempsStr += subject;
    tempsStr += F("\r\n\r\n");
    tempsStr += message;

    String payload = F("{\"raw\": \"");
    payload += base64::encode(tempsStr);
    payload += F("\"}");

    m_auth->sendCommand("POST ", GMAIL_HOST, "/gmail/v1/users/me/messages/send", payload.c_str(), true);
    return readGMailClient(SEND_EMAIL);
}


String GoogleGmailAPI::readSnippet(const char* idEmail){
    return readMail( idEmail, true);
}


String GoogleGmailAPI::readMail(const char* idEmail, bool snippet){
    String cmd = F("/gmail/v1/users/me/messages/");
    cmd += idEmail;
    m_auth->sendCommand("GET ", GMAIL_HOST, cmd.c_str(), "", true);
    if (snippet)
        return readGMailClient(READ_SNIPPET, idEmail);
    else
        return readGMailClient(READ_EMAIL, idEmail);
}

