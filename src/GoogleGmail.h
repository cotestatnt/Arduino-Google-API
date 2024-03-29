#ifndef GOOGLEO_GMAIL_API
#define GOOGLEO_GMAIL_API

#include "GoogleOAuth2.h"

#define GMAIL_HOST "gmail.googleapis.com"

class GmailList
{
    struct mailItem
    {
        String id;
        String from;
        String subject;
        String date;
        bool read = false;
        mailItem *nextItem = nullptr;
    };

    friend class GoogleGmailAPI;

public:
    ~GmailList()
    {
        mailItem *p_item = m_firstItem;
        while (p_item != nullptr)
        {
            /* Get the next file pointer before destroying the obj */
            mailItem *p_next_item = p_item->nextItem;

            /* Delete the obj */
            delete p_item;

            /* Move to next obj */
            p_item = p_next_item;
        }
    };

    void clear()
    {
        m_firstItem = nullptr;
        m_lastItem = nullptr;
    }

    unsigned int size() const
    {
        return m_mailCount;
    }

    void addMailId(const char *id, bool read)
    {
        for (mailItem *item = m_firstItem; item != nullptr; item = item->nextItem)
        {
            if (item->id.equals(id))
                return; //  id already present in list
        }
        mailItem *thisItem = new mailItem();
        if (m_firstItem != nullptr)
            m_lastItem->nextItem = thisItem;
        else
            m_firstItem = thisItem;
        m_lastItem = thisItem;
        m_mailCount++;
        thisItem->read = read;
        thisItem->id = id;
    }

    const char *getMailId(int index) const
    {
        int counter = 0;
        for (mailItem *item = m_firstItem; item != nullptr; item = item->nextItem)
        {
            if (counter == index)
                return item->id.c_str();
            counter++;
        }
        return "not found";
    }

    const char *getFrom(const char *id) const
    {
        mailItem *item = getItem(id);
        if (item != nullptr)
        {
            return item->from.c_str();
        }
        return "not found";
    }

    const char *getDate(const char *id) const
    {
        mailItem *item = getItem(id);
        if (item != nullptr)
            return item->date.c_str();
        return 0;
    }

    const char *getSubject(const char *id) const
    {
        mailItem *item = getItem(id);
        if (item != nullptr)
            return item->subject.c_str();
        return "not found";
    }

    void printList() const
    {
        for (mailItem *item = m_firstItem; item != nullptr; item = item->nextItem)
        {
            Serial.print("\tid: ");
            Serial.println(item->id);
        }
    }

    mailItem *getItem(const char *id) const
    {
        for (mailItem *item = m_firstItem; item != nullptr; item = item->nextItem)
            if (item->id.equals(id))
                return item;
        return nullptr;
    }

private:
    unsigned int m_mailCount;
    mailItem *m_firstItem = nullptr;
    mailItem *m_lastItem = nullptr;
};

class GoogleGmailAPI
{
public:
    GoogleGmailAPI(GoogleOAuth2 *auth, GmailList *list = nullptr);
    ~GoogleGmailAPI() { delete m_auth; };

    // Send a new email from ESP
    String sendEmail(const char *to, const char *subject, const char *message);
    String sendEmail(const String &to, const String &subject, const String &message)
    {
        return sendEmail(to.c_str(), subject.c_str(), message.c_str());
    }

    // Return the number of unreaded messages in list
    int getMailList(const char *from = nullptr, bool unread = true, uint32_t maxResults = 100);

    // Return the body of message as snippet or as complete message plain text
    String readMail(const char *idEmail, bool snippet = false);
    String readSnippet(const char *idEmail);

    // Get the metadata for message id
    int getMailData(const char *idEmail);

    // Set a specific message id as read
    bool setMessageRead(const char *idEmail);

private:
    enum
    {
        MAIL_ID,
        MAIL_LIST,
        READ_SNIPPET,
        READ_EMAIL,
        SEND_EMAIL
    };

    GoogleOAuth2 *m_auth = nullptr;
    GmailList *m_mailList = nullptr;

    // Class specialized parser
    String readGMailClient(const int filter, const char *keyword = nullptr);
    String parsePayload(const String &payload, const int filter, const char *gFile);
};

#endif
