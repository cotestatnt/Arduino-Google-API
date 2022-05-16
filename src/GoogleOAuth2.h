#include "Arduino.h"
#include <ArduinoJson.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif
#include <FS.h>
#include <WiFiClientSecure.h>

#ifndef DEBUG_ENABLE
#define DEBUG_ENABLE 1
#endif

#ifndef DEBUG_FUNCTION_CALL
#define DEBUG_FUNCTION_CALL 1
#endif
#include "SerialLog.h"

#ifndef GOOGLE_OAUTH2
#define GOOGLE_OAUTH2

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

#define MIN_ID_LEN 33

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
    int getState();
    void printConfig() const;
    void clearConfig() const;
    const char *getUserCode() const;

    inline bool isAuthorized()
    {
        return (getState() == GOT_TOKEN);
    }

protected:
    Client *m_ggclient;
    String m_user_code = "";
    String m_device_code = "";
    String m_token_type = "";

    fs::FS *m_filesystem;
    char *m_localhost;
    int m_ggstate;
    uint32_t m_expires_at_ms;
    const char *m_configFile = "/gapi_config.json";

    /* connect to Google server */
    bool doConnection(const char *host) const;

    /* send an HTTP command to Google server */
    void sendCommand(const char *const &rest, const char *const &host,
                     const char *const &command, const char *const &body, bool bearer);

    // Return true if data succesfull parsed
    bool readggClient(String &payload, bool keep_connection = false);

    // Class specialized parser
    bool parsePayload(const String &payload);

    /* return true if access token is valid */
    bool isAccessTokenValid();

    /* return true if refresh access token succesfully */
    bool refreshToken(bool stopClient = false); // return true on access token validù

    /* get from Google the device code */
    bool requestDeviceCode();

    /* wait for a reply from Google server during authorization worlkflow */
    bool pollingAuthorize();

    /* Check if access token is still valid (local time) */
    void checkRefreshTime();

    /* load app data configuration from filesystem (gapi_config.json") */
    bool loadConfig(const char *id, const char *secret, const char *scope, const char *api_key, const char *redirect_uri) const;

    /* update a single parameter in gapi_config-json file */
    bool writeParam(const char *keyword, const char *value) const;

    /* read a single parameter from gapi_config-json file */
    String readParam(const char *keyword) const;

    /* inflate gzipped payload received from Google server */
    String gzipInflate(const String &compressedBytes) const;
};

#endif
