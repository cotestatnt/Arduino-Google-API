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
		uint32_t free; uint16_t max; uint8_t frag; ESP.getHeapStats(&free, &max, &frag); Serial.printf("free: %5d - max: %5d - frag: %3d%% <- ", free, max, frag);\
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
#include "include/json/JSONStreamingParser.h"
#include "include/PString/PString.h"

#define AUTH_HOST "oauth2.googleapis.com"
#define PORT 443

class GoogleOAuth2
{

friend void __JSONCallback(uint8_t filter, uint8_t level, const char *name, const char *value, void *cbObj);

public:
    enum { INIT, REQUEST_AUTH, GOT_TOKEN, REFRESH_TOKEN };

    GoogleOAuth2(fs::FS *fs);
    GoogleOAuth2(fs::FS *fs, const char *configFile);
    bool begin(const char *id, const char *secret, const char *_scope);
    bool begin();
    int  getState();
    void printConfig();
    void clearConfig();

	/*
    *  true if access token is still valid otherwise refresk token is necessary
    *  if false, call refreshToken and then callback the function_cb if defined
    */
    bool isAccessTokenValid();
    
    bool writeParam(const char * keyword, const char * value );

protected:
#ifdef ESP8266 
    BearSSL::WiFiClientSecure ggclient;
#elif defined(ESP32)
    WiFiClientSecure ggclient;
#else

#endif

    JSONStreamingParser _parser;
    void parseJson(uint8_t filter, uint8_t level, const char *name, const char *value);

    const char* sendCommand(const char *const &rest, const char *const &host, const char *const &command,
                            const char *const &body, bool bearer );
    void readggClient(const char* funcName, const char* key, bool keep_connection = false);

    bool doConnection( const char *host);
    bool refreshToken();         // return true on access token valid
    bool requestDeviceCode();    // return true on request success
    bool pollingAuthorize();     // true on app authorized

    const char *_configFile = "/oauth2.json";
    bool loadConfig(const char *id, const char *secret, const char *_scope);
    const char * readParam(const char * keyword);

    fs::FS *_filesystem;
    char* localhost;
    int _ggstate;
    uint32_t expires_at_ms;    
};

#endif
