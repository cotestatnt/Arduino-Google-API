#define DEBUG_MODE false
#if DEBUG_MODE
#define serialLog(x) Serial.print(x)
#define serialLogln(x) Serial.println(x)
#define SerialBegin(x) Serial.begin(x)
#else
#define serialLog(x)
#define serialLogln(x)
#define SerialBegin(x)
#endif

#define DEBUG_F false
#if DEBUG_F
	#ifdef ESP32
		#define functionLog() { \
		Serial.printf("Heap memory %6d / %6d", heap_caps_get_free_size(0), heap_caps_get_largest_free_block(0));\
		Serial.print("\t\t\t--- "); Serial.print(millis()); Serial.print("mS > ");  Serial.print(__func__); Serial.println("()"); }
	#elif defined(ESP8266)
		#define functionLog() { \
		uint32_t free; uint16_t max; uint8_t frag; ESP.getHeapStats(&free, &max, &frag); Serial.printf("free: %5d - max: %5d <- ", free, max);\
		Serial.print("\t\t\t--- "); Serial.print(millis()); Serial.print("mS > ");  Serial.print(__func__); Serial.println("()"); }
	#endif
#else
    #define functionLog()
#endif


#ifndef GOOGLEOAUTH2
#define GOOGLEOAUTH2

#ifdef ESP8266
  #include <ESP8266WiFi.h>
  #include <WiFiClientSecure.h>

#elif defined(ESP32)
  #include <WiFiClientSecure.h>
#else
 #include <SPI.h>
 #include <Ethernet.h>
#endif

#include <FS.h>
#include "EspWebServer/EspWebServer.h"

#define AUTH_HOST "oauth2.googleapis.com"
#define PORT 443

class GoogleOAuth2
{

public:
    enum { INIT, REQUEST_AUTH, GOT_TOKEN, REFRESH_TOKEN, INVALID };
    GoogleOAuth2(fs::FS *fs);
    bool begin(const char *id, const char *secret, const char *_scope, const char *api_key, const char* redirect_uri);
    bool begin();
    int  getState();
    void printConfig() const;
    void clearConfig();
	const char* getUserCode();
    inline void run(){
        m_webserver->run();
    }

protected:
    EspWebServer* m_webserver;
    bool doConnection( const char *host);
    void sendCommand(const char *const &rest, const char *const &host,
                        const char *const &command, const char *const &body, bool bearer );
    const char* readggClient(bool keep_connection = false);

    bool isAccessTokenValid();
    bool refreshToken( bool stopClient = false);         // return true on access token valid
    bool requestDeviceCode();                            // return true on request success
    bool pollingAuthorize();                             // true on app authorized
    void checkRefreshTime();                             // Check if access token is still valid (local time)

    bool loadConfig(const char* id, const char* secret, const char* scope, const char *api_key, const char* redirect_uri);
    bool writeParam(const char* keyword, const char* value );
    const String readParam(const char* keyword) const;

    String parseLine(String &line, const char *filter) ;
    const String getValue(String &line, const char* param) const;

#if defined(ESP32)
    WiFiClientSecure m_ggclient;
#else
    BearSSL::Session m_session;
    BearSSL::WiFiClientSecure m_ggclient;
#endif

    char*       m_user_code;
    char*       m_device_code;

    fs::FS*     m_filesystem;
    char*       m_localhost;
    int         m_ggstate;
    uint32_t    m_expires_at_ms;
    const char* m_configFile = "/gapi_config.json";
};

#endif
