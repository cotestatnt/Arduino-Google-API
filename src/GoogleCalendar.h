#ifndef GOOGLE_CALENDAR_API
#define GOOGLE_CALENDAR_API

#include "GoogleOAuth2.h"

#define CALENDAR_HOST "www.googleapis.com"

class EventList
{
    friend class GoogleCalendar;

public:
    struct EventItem
    {
        String id;
        String summary;
        time_t start;
        time_t stop;
        EventItem *nextItem = nullptr;
    };

    ~EventList()
    {
        EventItem *p_item = m_firstItem;
        while (p_item != nullptr)
        {
            /* Get the next file pointer before destroying the obj */
            EventItem *p_next_item = p_item->nextItem;

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
        return m_eventCount;
    }

    void addEvent(const char *id, const char *summary, time_t start, time_t stop)
    {
        for (EventItem *item = m_firstItem; item != nullptr; item = item->nextItem)
        {
            if (item->id.equals(id))
                return; //  id already present in list
        }
        EventItem *thisItem = new EventItem();
        if (m_firstItem != nullptr)
            m_lastItem->nextItem = thisItem;
        else
            m_firstItem = thisItem;
        m_lastItem = thisItem;
        m_eventCount++;
        thisItem->id = id;
        thisItem->summary = summary;
        thisItem->start = start;
        thisItem->stop = stop;
    }

    const char *getEventId(int index) const
    {
        int counter = 0;
        for (EventItem *item = m_firstItem; item != nullptr; item = item->nextItem)
        {
            if (counter == index)
                return item->id.c_str();
            counter++;
        }
        return "not found";
    }

    uint32_t getStartTime(const char *id) const
    {
        EventItem *item = getEvent(id);
        if (item != nullptr)
            return item->start;
        return 0;
    }

    uint32_t getStopTime(const char *id) const
    {
        EventItem *item = getEvent(id);
        if (item != nullptr)
            return item->stop;
        return 0;
    }

    const char *getSummary(const char *id) const
    {
        EventItem *item = getEvent(id);
        if (item != nullptr)
            return item->summary.c_str();
        return "not found";
    }

    void printList() const
    {
        for (EventItem *item = m_firstItem; item != nullptr; item = item->nextItem)
        {
            Serial.print("Event id: ");
            Serial.print(item->id);
            Serial.print("\tsummary: ");
            Serial.print(item->summary);
            Serial.print("\tstart: ");
            Serial.print(item->start);
            Serial.print("\tend: ");
            Serial.println(item->stop);
        }
    }

    EventItem *getEvent(const char *id) const
    {
        for (EventItem *item = m_firstItem; item != nullptr; item = item->nextItem)
            if (item->id.equals(id))
                return item;
        return nullptr;
    }

    EventItem *getEvent(int index) const
    {
        int counter = 0;
        for (EventItem *item = m_firstItem; item != nullptr; item = item->nextItem) {
            if (index == counter++)
                return item;
        }
        return nullptr;
    }


private:
    unsigned int m_eventCount;
    EventItem *m_firstItem = nullptr;
    EventItem *m_lastItem = nullptr;
};

class GoogleCalendarAPI
{
public:
    enum
    {
        NONE,
        DAILY,
        WEEKLY,
        MONTHLY,
        YEARLY
    };
    const char Freq[5][10] = {"", "DAILY", "WEEKLY", "MONTHLY", "YEARLY"};

    /* Constructor that receive const reference to 'GoogleOAuth2' object and a reference to 'EventList'*/
    GoogleCalendarAPI(GoogleOAuth2 &auth, EventList &list) : m_auth(auth), m_eventList(list) {}

    /* Destructor, does nothing */
    ~GoogleCalendarAPI() {}

    /* Check for a specific calendar with proved summary, and optionally create */
    const char *checkCalendar(const char *calendarSummary, bool create = true, const char *timeZone = nullptr);
    inline const char *checkCalendar(const String &calendarSummary, bool create, const char *timeZone)
    {
        return checkCalendar(calendarSummary.c_str(), create, timeZone);
    }

    /* Get the list of events in the specified time range (RFC3339 timestamp with mandatory time zone offset) */
    int getEventList(const char *calendarId, const char *timeMin, const char *timeMax);
    inline int getEventList(const String &calendarId, const char *timeMin, const char *timeMax)
    {
        return getEventList(calendarId.c_str(), timeMin, timeMax);
    }

    /* Get the list of events in the specified time range (EPOCH timestamp) */
    int getEventList(const char *calendarId, time_t timeMin = 0, time_t timeMax = 0);
    inline int getEventList(const String &calendarId, time_t timeMin = 0, time_t timeMax = 0)
    {
        return getEventList(calendarId.c_str(), timeMin, timeMax);
    }

    inline void prinEventList()
    {
        m_eventList.printList();
    }

    /* Add a new event to specified calendar, return the id of new event
       optional recurrency rules can be passed as array of string eg: "RRULE:FREQ=DAILY;COUNT=2"
    */
    const char *addEvent(const char *calendarId, const char *summary, time_t start, time_t end, const char *recurrency = nullptr);

    /* Add a new event to specified calendar, return the id of new event
       optional recurrency rules can be passed as parameter eg: DAILY, 2 times
    */
    const char *addEvent(const char *calendarId, const char *summary, time_t start, time_t end, int recurrency = 0, int count = 0);

    /* Creates an event based on a simple text string, return the id of new event
        example sentence: "Do somenthing at Somewhere on June 3rd 10am-10:25am"
    */
    const char *quickAddEvent(const char *calendarId, const char *sentence);
    inline const char *quickAddEvent(const String& calendarId, const String& sentence)
    {
        return quickAddEvent(calendarId.c_str(), sentence.c_str());
    }

    /* Deletes an event of a specific calendar */
    bool deleteEvent(const char *calendarId, const char* eventId);
    inline bool deleteEvent(const String& calendarId, const String& eventId)
    {
        return deleteEvent(calendarId.c_str(), eventId.c_str());
    }

    /* Utility method for getting a time_t variable from specific datetime */
    inline time_t getEpochTime(int year, int month, int day, int hours, int minutes, int seconds = 0) {
        struct tm ts;
        getLocalTime(&ts);
        //ts.tm_isdst = 1;
     	ts.tm_sec = seconds;
        ts.tm_min = minutes;
        ts.tm_hour = hours;
        ts.tm_mday = day;
        ts.tm_mon = month - 1;
        ts.tm_year = year - 1900;
        return mktime(&ts);
    }

    inline EventList::EventItem* getEvent(int index) {
        return m_eventList.getEvent(index);
    }

    inline bool eventInProgress(int index) {
        EventList::EventItem* event = getEvent(index);
        time_t now = time(nullptr);
        bool res = false;
        if (event != nullptr)
            res = now >= event->start && now <= event->stop;
        return res;
    }

private:
    enum
    {
        CALENDARS,
        EVENTS,
        NEW_CALENDAR,
        NEW_EVENT,
        DELETE_EVENT
    };

    /* Default constructor is declared but NOT implemented */
    GoogleCalendarAPI();

    GoogleOAuth2 & m_auth;
    EventList & m_eventList;

    String m_calendarId = "";
    String m_calendarTimeZone = "";

    // Class specialized parser
    String readCalendarClient(const int filter, const char *keyword = nullptr);
    String parsePayload(const String &payload, const int filter, const char *gFile);

    String createRRule(int recurType, int count);
};

#endif
