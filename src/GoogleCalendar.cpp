#include "GoogleCalendar.h"
#include "Base64.h"

GoogleCalendarAPI::GoogleCalendarAPI(GoogleOAuth2 *auth, EventList *list)
{
    if (list != nullptr)
        m_eventList = list;
    m_auth = auth;
}

String GoogleCalendarAPI::parsePayload(const String &payload, const int filter, const char *keyword)
{
#if defined(ESP8266)
    DynamicJsonDocument doc(ESP.getMaxFreeBlockSize() - 512);
#elif defined(ESP32)
    DynamicJsonDocument doc(ESP.getMaxAllocHeap() - 512);
#else
    DynamicJsonDocument doc(payload.length() + 256);
#endif
    DeserializationError err = deserializeJson(doc, payload);
    if (err)
    {
        log_error("deserializeJson() failed\n");
        log_error("%s\n", err.c_str());
        return "";
    }

    String result = "";
    switch (filter)
    {
    case CALENDARS:
        if (doc["items"])
        {
            JsonArray array = doc["items"].as<JsonArray>();
            for (JsonVariant calendar : array)
            {
                if (calendar["summary"].as<String>().equals(keyword)) {
                    result = calendar["id"].as<String>();
                    m_calendarTimeZone = calendar["timeZone"].as<String>();
                }
            }
        }
        break;

    case NEW_CALENDAR:
        if (doc["summary"].as<String>().equals(keyword))
        {
            result = doc["id"].as<String>();
        }
        break;


    case EVENTS:
        if (doc["items"])
        {
            JsonArray array = doc["items"].as<JsonArray>();
            for (JsonVariant event : array)
            {
                const char* id = event["id"];
                const char* summary = event["summary"];
                const char* start_str = event["start"]["dateTime"];
                const char* end_str = event["end"]["dateTime"];
                time_t start;
                time_t end;

                struct tm ts;
                strptime(start_str, "%FT%T%z", &ts);
                start = mktime(&ts);

                strptime(end_str, "%FT%T%z", &ts);
                end = mktime(&ts);
                m_eventList->addEvent(id, summary, start, end);
            }
        }
        result = m_eventList->size();
        break;

    case NEW_EVENT:
        result = doc["id"].as<String>();
        break;

    case DELETE_EVENT:
        result = doc["noContent"].as<String>();
        break;
    }
    return result;
}

String GoogleCalendarAPI::readCalendarClient(const int filter, const char *keyword)
{
    String payload;
    m_auth->readggClient(payload, true, false);
    return parsePayload(payload, filter, keyword);
}

const char *GoogleCalendarAPI::checkCalendar(const char *calendarSummary, bool create, const char* timeZone)
{
    m_calendarId = "";
    m_auth->sendCommand("GET ", CALENDAR_HOST, "/calendar/v3/users/me/calendarList", "", true);
    m_calendarId = readCalendarClient(CALENDARS, calendarSummary);
    /* calendar not present, create a new one */
    if (!m_calendarId.length() && create)
    {
        String body = "{\"summary\": \"";
        body += calendarSummary;
        if (timeZone != nullptr) {
            body += "\", \"timeZone\":\"";
            body += timeZone;
            m_calendarTimeZone = timeZone;
        }
        body += "\"}";
        m_auth->sendCommand("POST ", CALENDAR_HOST, "/calendar/v3/calendars", body.c_str(), true);
        m_calendarId = readCalendarClient(NEW_CALENDAR, calendarSummary);
    }
    return m_calendarId.c_str();
}

int GoogleCalendarAPI::getEventList(const char *calendarId, const char *timeMin, const char *timeMax)
{
    String request = "/calendar/v3/calendars/";
    request += m_auth->urlencode(calendarId);
    request += "/events";
    if (timeMin != nullptr&& strlen(timeMin))
    {
        request += "?timeMin=";
        request += m_auth->urlencode(timeMin);
        if (timeMax != nullptr && strlen(timeMax))
        {
            request += "&timeMax=";
            request += m_auth->urlencode(timeMax);
        }
    }
    m_auth->sendCommand("GET ", CALENDAR_HOST, request.c_str(), "", true);
    String res = readCalendarClient(EVENTS, calendarId);
    return res.toInt();
}

int GoogleCalendarAPI::getEventList(const char *calendarId, time_t timeMin, time_t timeMax)
{
    char min[25 + 1]; // RFC3339 timestamp
    char max[25 + 1]; // RFC3339 timestamp
    struct tm ts;

    memset(min, '\0', sizeof(min));
    memset(max, '\0', sizeof(max));

    if (timeMin != 0)
    {
        localtime_r(&timeMin, &ts);
        strftime(min, sizeof(min), "%FT%T%z", &ts);
        if (timeMax != 0)
        {
            localtime_r(&timeMax, &ts);
            strftime(max, sizeof(max), "%FT%T%z", &ts);
        }
    }
    else {
        time(&timeMin);
        localtime_r(&timeMin, &ts);
        strftime(min, sizeof(min), "%FT%T%z", &ts);
    }
    return getEventList(calendarId, min, max);
}


String GoogleCalendarAPI::createRRule(int recurType, int count)
{
    String rule = "RRULE:FREQ=";
    rule += Freq[recurType];
    rule += ";COUNT=";
    rule += count;
    return rule;
}



const char* GoogleCalendarAPI::addEvent(const char* calendarId, const char* summary, time_t start, time_t end, const char* recurrency)
{
    char sentence[128];
    char startStr[25+1];
    char endStr[25+1];

    struct tm ts;
    localtime_r(&start, &ts);
    strftime(startStr, sizeof(startStr), "%FT%T%z", &ts);
    localtime_r(&end, &ts);
    strftime(endStr, sizeof(endStr), "%FT%T%z", &ts);

    m_calendarId = "";
    String request = "/calendar/v3/calendars/";
    request += m_auth->urlencode(calendarId);
    request += "/events";

    String body = "{\"summary\": \"";
    body += summary;
    body += "\",\"start\":{\"dateTime\":\"";
    body += startStr;
    body += "\", \"timeZone\":\"";
    body += m_calendarTimeZone;
    body += "\"},\"end\":{\"dateTime\":\"";
    body += endStr;
    body += "\", \"timeZone\":\"";
    body += m_calendarTimeZone;
    if (recurrency) {
        body += "\"},\"recurrence\":[\"";
        body += recurrency;
        body += "\"]}";
    }
    else {
        body += "\"}}";
    }
    m_auth->sendCommand("POST ", CALENDAR_HOST, request.c_str(), body.c_str(), true);
    m_calendarId = readCalendarClient(NEW_EVENT);

    return m_calendarId.c_str();
}

const char* GoogleCalendarAPI::addEvent(const char* calendarId, const char* summary, time_t start, time_t end, int recurrency, int count)
{
    String rule = createRRule(recurrency, count);
    return addEvent(calendarId, summary, start, end, rule.c_str());
}


const char* GoogleCalendarAPI::quickAddEvent(const char* calendarId,  const char* sentence)
{
    m_calendarId = "";
    String request = "/calendar/v3/calendars/";
    request += m_auth->urlencode(calendarId);
    request += "/events/quickAdd?text=";
    request += m_auth->urlencode(sentence);

    m_auth->sendCommand("POST ", CALENDAR_HOST, request.c_str(), "{}", true);
    m_calendarId = readCalendarClient(NEW_EVENT);

    return m_calendarId.c_str();
}

bool GoogleCalendarAPI::deleteEvent(const char* calendarId,  const char* eventId)
{
    // DELETE https://www.googleapis.com/calendar/v3/calendars/calendarId/events/eventId
    m_calendarId = "";
    String request = "/calendar/v3/calendars/";
    request += m_auth->urlencode(calendarId);
    request += "/events/";
    request += eventId;

    m_auth->sendCommand("DELETE ", CALENDAR_HOST, request.c_str(), "", true);
    m_calendarId = readCalendarClient(DELETE_EVENT);
    return m_calendarId.equals("true");;
}