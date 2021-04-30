#include "Arduino.h"

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

static const char google_cert[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDujCCAqKgAwIBAgILBAAAAAABD4Ym5g0wDQYJKoZIhvcNAQEFBQAwTDEgMB4G
A1UECxMXR2xvYmFsU2lnbiBSb290IENBIC0gUjIxEzARBgNVBAoTCkdsb2JhbFNp
Z24xEzARBgNVBAMTCkdsb2JhbFNpZ24wHhcNMDYxMjE1MDgwMDAwWhcNMjExMjE1
MDgwMDAwWjBMMSAwHgYDVQQLExdHbG9iYWxTaWduIFJvb3QgQ0EgLSBSMjETMBEG
A1UEChMKR2xvYmFsU2lnbjETMBEGA1UEAxMKR2xvYmFsU2lnbjCCASIwDQYJKoZI
hvcNAQEBBQADggEPADCCAQoCggEBAKbPJA6+Lm8omUVCxKs+IVSbC9N/hHD6ErPL
v4dfxn+G07IwXNb9rfF73OX4YJYJkhD10FPe+3t+c4isUoh7SqbKSaZeqKeMWhG8
eoLrvozps6yWJQeXSpkqBy+0Hne/ig+1AnwblrjFuTosvNYSuetZfeLQBoZfXklq
tTleiDTsvHgMCJiEbKjNS7SgfQx5TfC4LcshytVsW33hoCmEofnTlEnLJGKRILzd
C9XZzPnqJworc5HGnRusyMvo4KD0L5CLTfuwNhv2GXqF4G3yYROIXJ/gkwpRl4pa
zq+r1feqCapgvdzZX99yqWATXgAByUr6P6TqBwMhAo6CygPCm48CAwEAAaOBnDCB
mTAOBgNVHQ8BAf8EBAMCAQYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUm+IH
V2ccHsBqBt5ZtJot39wZhi4wNgYDVR0fBC8wLTAroCmgJ4YlaHR0cDovL2NybC5n
bG9iYWxzaWduLm5ldC9yb290LXIyLmNybDAfBgNVHSMEGDAWgBSb4gdXZxwewGoG
3lm0mi3f3BmGLjANBgkqhkiG9w0BAQUFAAOCAQEAmYFThxxol4aR7OBKuEQLq4Gs
J0/WwbgcQ3izDJr86iw8bmEbTUsp9Z8FHSbBuOmDAGJFtqkIk7mpM0sYmsL4h4hO
291xNBrBVNpGP+DTKqttVCL1OmLNIG+6KYnX3ZHu01yiPqFbQfXf5WRDLenVOavS
ot+3i9DAgBkcRcAtjOj4LaR0VknFBbVPFd5uRHg5h6h+u/N5GJG79G+dwfCMNYxd
AfvDbbnvRG15RjF+Cv6pgsH/76tuIMRQyV+dTZsXjAzlAcmgQWpzU/qlULRuJQ/7
TBj0/VLZjmmx6BEP3ojY+x1J96relc8geMJgEtslQIxq/H5COEBkEveegeGTLg==
-----END CERTIFICATE-----
)EOF";


#include <FS.h>
#include "GapiServer.h"

#define AUTH_HOST "oauth2.googleapis.com"
#define PORT 443

class GoogleOAuth2
{

public:
    enum { INIT, REQUEST_AUTH, GOT_TOKEN, REFRESH_TOKEN, INVALID };
    GoogleOAuth2(fs::FS &fs, Client &client);
    bool begin(const char *id, const char *secret, const char *_scope, const char *api_key, const char* redirect_uri);
    bool begin();
    int  getState();
    void printConfig() const;
    void clearConfig();
	const char* getUserCode();


protected:

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
    Client *m_ggclient;

// #if defined(ESP32)
//     WiFiClientSecure m_ggclient;
// #else
//     BearSSL::Session m_session;
//     BearSSL::WiFiClientSecure m_ggclient;
// #endif

    char*       m_user_code;
    char*       m_device_code;

    fs::FS*     m_filesystem;
    char*       m_localhost;
    int         m_ggstate;
    uint32_t    m_expires_at_ms;
    const char* m_configFile = "/gapi_config.json";
};

#endif
