#include "Arduino.h"
#include <ArduinoJson.h>

#if defined(ESP8266)
    #include <ESP8266WiFi.h>
#elif defined(ESP32)
    #include <WiFi.h>
#endif
#include <FS.h>
#include <WiFiClientSecure.h>

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
MIIDdTCCAl2gAwIBAgILBAAAAAABFUtaw5QwDQYJKoZIhvcNAQEFBQAwVzELMAkG
A1UEBhMCQkUxGTAXBgNVBAoTEEdsb2JhbFNpZ24gbnYtc2ExEDAOBgNVBAsTB1Jv
b3QgQ0ExGzAZBgNVBAMTEkdsb2JhbFNpZ24gUm9vdCBDQTAeFw05ODA5MDExMjAw
MDBaFw0yODAxMjgxMjAwMDBaMFcxCzAJBgNVBAYTAkJFMRkwFwYDVQQKExBHbG9i
YWxTaWduIG52LXNhMRAwDgYDVQQLEwdSb290IENBMRswGQYDVQQDExJHbG9iYWxT
aWduIFJvb3QgQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDaDuaZ
jc6j40+Kfvvxi4Mla+pIH/EqsLmVEQS98GPR4mdmzxzdzxtIK+6NiY6arymAZavp
xy0Sy6scTHAHoT0KMM0VjU/43dSMUBUc71DuxC73/OlS8pF94G3VNTCOXkNz8kHp
1Wrjsok6Vjk4bwY8iGlbKk3Fp1S4bInMm/k8yuX9ifUSPJJ4ltbcdG6TRGHRjcdG
snUOhugZitVtbNV4FpWi6cgKOOvyJBNPc1STE4U6G7weNLWLBYy5d4ux2x8gkasJ
U26Qzns3dLlwR5EiUWMWea6xrkEmCMgZK9FGqkjWZCrXgzT/LCrBbBlDSgeF59N8
9iFo7+ryUp9/k5DPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNVHRMBAf8E
BTADAQH/MB0GA1UdDgQWBBRge2YaRQ2XyolQL30EzTSo//z9SzANBgkqhkiG9w0B
AQUFAAOCAQEA1nPnfE920I2/7LqivjTFKDK1fPxsnCwrvQmeU79rXqoRSLblCKOz
yj1hTdNGCbM+w6DjY1Ub8rrvrTnhQ7k4o+YviiY776BQVvnGCv04zcQLcFGUl5gE
38NflNUVyRRBnMRddWQVDf9VMOyGj/8N7yy5Y0b2qvzfvGn9LhJIZJrglfCm7ymP
AbEVtQwdpf5pLGkkeB6zpxxxYu7KyJesF12KwvhHhm4qxFYxldBniYUr+WymXUad
DKqC5JlR3XC321Y9YeRq4VzW9v493kHMB65jUr9TU/Qr6cf9tveCX4XSQRjbgbME
HMUfpIBvFSDJ3gyICh3WZlXi/EjJKSZp4A==
-----END CERTIFICATE-----
)EOF";




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
    inline bool isAuthorized() {
        return (getState() == GOT_TOKEN);
    }

protected:

    bool doConnection( const char *host);
    void sendCommand(const char *const &rest, const char *const &host,
                        const char *const &command, const char *const &body, bool bearer );
    void readggClient(String &payload, bool keep_connection = false);

    bool isAccessTokenValid();
    bool refreshToken( bool stopClient = false);         // return true on access token valid
    bool requestDeviceCode();                            // return true on request success
    bool pollingAuthorize();                             // true on app authorized
    void checkRefreshTime();                             // Check if access token is still valid (local time)

    bool loadConfig(const char* id, const char* secret, const char* scope, const char *api_key, const char* redirect_uri);
    bool writeParam(const char* keyword, const char* value );
    const String readParam(const char* keyword) const;

    String parseLine(String &line) ;
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
