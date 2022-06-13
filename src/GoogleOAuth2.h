#include "Arduino.h"
#include <ArduinoJson.h>
#include <esp-fs-webserver.h>   // https://github.com/cotestatnt/esp-fs-webserver

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif
#include <FS.h>
#include <WiFiClientSecure.h>

#ifndef GOOGLE_OAUTH2
#define GOOGLE_OAUTH2

#ifndef DEBUG_ENABLE
#define DEBUG_ENABLE 1
#endif

#ifndef DEBUG_FUNCTION_CALL
#define DEBUG_FUNCTION_CALL 0
#endif

#include "SerialLog.h"
#include "Const.h"

class GoogleOAuth2
{

public:

    enum
    {
        INIT,
        REQUEST_AUTH,
        GOT_TOKEN,
        REFRESH_TOKEN,
        INVALID
    };

    GoogleOAuth2(fs::FS &fs, Client &client);
    ~GoogleOAuth2(){};

    bool begin(const char *id, const char *secret, const char *_scope, const char *api_key, const char *redirect_uri);
    bool begin();
    unsigned int getState();
    void printConfig() const;
    void clearConfig() const;
    const char *getUserCode() const;

    inline bool isAuthorized()
    {
        return (getState() == GOT_TOKEN);
    }

    /* send an HTTP command to Google server */
    void sendCommand(const char *const &rest, const char *const &host,
                     const char *const &command, const char *const &body, bool bearer);

    /* Return true if data succesfull parsed */
    bool readggClient(String &payload, bool keep_connection = false, bool payload_gzip = true);

    /* read a single parameter from gapi_config-json file */
    const String readParam(const char *keyword) const;

    /* connect to Google server */
    bool doConnection(const char *host) const;

    /* Check if access token is still valid (local time) */
    void checkRefreshTime();

    /* Pointer to WiFi or Ethernet */
    Client *m_ggclient;

    /* Pointer to filesystem  (where configuration files are stored) */
    fs::FS *m_filesystem;

    /* encode url string utility */
    String urlencode(const char *str);

private:
    const char *m_configFile = "/gapi_config.json";
    uint32_t m_expires_at_ms = 0;
    unsigned int m_ggstate = INIT;

    String m_user_code = "";
    String m_device_code = "";
    String m_token_type = "";

    /*  Class specialized parser */
    bool parsePayload(const String &payload);

    /* return true if access token is valid */
    bool isAccessTokenValid();

    /* return true if refresh access token succesfully */
    bool refreshToken(bool stopClient = false); // return true on access token validù

    /* get from Google the device code */
    bool requestDeviceCode();

    /* wait for a reply from Google server during authorization worlkflow */
    bool pollingAuthorize();

    /* load app data configuration from filesystem (gapi_config.json") */
    bool loadConfig(const char *id, const char *secret, const char *scope, const char *api_key, const char *redirect_uri) const;

    /* update a single parameter in gapi_config-json file */
    bool writeParam(const char *keyword, const char *value) const;

    /* inflate gzipped payload received from Google server */
    String gzipInflate(const String &compressedBytes) const;

};

#endif
