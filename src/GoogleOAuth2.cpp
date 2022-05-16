#include "GoogleOAuth2.h"
#include "zlib/zlib.h"
#include "zlib/zconf.h"

GoogleOAuth2::GoogleOAuth2(fs::FS &fs, Client &client)
{
    m_ggclient = &client;
    m_filesystem = &fs;
    m_ggstate = INIT;
}


bool GoogleOAuth2::begin()
{
    File file = m_filesystem->open(m_configFile, "r");
    if (!file || file.isDirectory())
    {
        Serial.print(F("Failed to read configuration file"));
        Serial.println(m_configFile);
        Serial.println(F("If you call begin() without parameter, a configuration file must be present in filesystem"));
        return false;
    }
    const char *id = readParam("client_id").c_str();
    const char *secret = readParam("client_secret").c_str();
    const char *_scope = readParam("scope").c_str();
    const char *api_key = readParam("key").c_str();
    const char *redirect_uri = readParam("redirect_uri").c_str();
    return begin(id, secret, _scope, api_key, redirect_uri);
}

bool GoogleOAuth2::begin(const char *id, const char *secret, const char *_scope, const char *api_key, const char *redirect_uri)
{
    // functionLog() ;
    m_expires_at_ms = 30000;

    // Load config file or create it if don't exist
    if (loadConfig(id, secret, _scope, api_key, redirect_uri))
    {
        if (!m_ggclient->connected())
            if (!doConnection(AUTH_HOST))
                return false;

        if (isAccessTokenValid())
        {
            m_ggstate = GOT_TOKEN;
            Serial.println(PSTR("\nAccess token valid"));
            return true;
        }
        else if (refreshToken(true))
        {
            Serial.println(PSTR("\nAccess token refreshed"));
            m_ggstate = GOT_TOKEN;
            return true;
        }
    }
    else
    {
        return false;
    }

    if (m_ggstate != GOT_TOKEN)
    {
        // No valid access token (is this the first time we authorize app?)
        Serial.println(PSTR("\nNo valid access or refresh token found in config file."));
        if (requestDeviceCode())
        {
            m_ggstate = REQUEST_AUTH;
            return true;
        }
        else
        {
            m_ggstate = INVALID;
            return false;
        }
    }
    // default value
    return false;
}

bool GoogleOAuth2::loadConfig(const char *id, const char *secret, const char *_scope, const char *api_key, const char *redirect_uri) const
{
    functionLog();
    if (!m_filesystem->exists(m_configFile))
    {
        File newfile = m_filesystem->open(m_configFile, "w");
        if (!newfile)
        {
            log_error("Failed to crete config file");
            return false;
        }
        newfile.print(F("{\r\n"));
        newfile.print(F("  \"redirect_uri\": \""));
        newfile.print(redirect_uri);
        newfile.print(F("\",  \r\n"));
        newfile.print(F("  \"scope\": \""));
        newfile.print(_scope);
        newfile.print(F("\",  \r\n"));
        newfile.print(F("  \"api_key\": \""));
        newfile.print(api_key);
        newfile.print(F("\",  \r\n"));
        newfile.print(F("  \"client_id\": \""));
        newfile.print(id);
        newfile.print(F("\",  \r\n"));
        newfile.print(F("  \"client_secret\": \""));
        newfile.print(secret);
        newfile.print(F("\"  \r\n}"));
        newfile.close();
        Serial.println(F("Config file created."));
        return true;
    }

    File file = m_filesystem->open(m_configFile, "r");
    if (!file)
    {
        log_error("Failed to open file\n");
        return false;
    }

    return true;
}

int GoogleOAuth2::getState()
{
    // functionLog() ;
    if (m_ggstate == REQUEST_AUTH && pollingAuthorize())
    {
        m_ggstate = GOT_TOKEN;
        log_info("Got token\n");
    }
    return m_ggstate;
}

bool GoogleOAuth2::doConnection(const char *host) const
{
    // functionLog() ;
    m_ggclient->stop();
    if (!m_ggclient->connected())
    {
        log_info("Start handshaking with host %s...", host);
        if (!m_ggclient->connect(host, PORT))
        {
            log_error(" unable to connect!\n");
        }
        else
        {
            log_info(" connected\n");
        }
    }
    return m_ggclient->connected();
}

void GoogleOAuth2::sendCommand(const char *const &rest, const char *const &host, const char *const &command,
                               const char *const &body, bool bearer = false)
{
    // functionLog() ;
    // Refresh token if necessary except if this is called from refreshToken()
    if (strcmp(command, "/token") != 0)
        checkRefreshTime();

    if (!m_ggclient->connected())
        if (!doConnection(host))
            return;
    log_debug("\nHTTP request to host %s\n%s%s HTTP/1.0\n%s\n", host, rest, command, body);

    m_ggclient->print(rest);
    m_ggclient->print(command);
    m_ggclient->print(F(" HTTP/1.0"));
    m_ggclient->print(F("\r\nHost: "));
    m_ggclient->print(host);
    m_ggclient->print(F("\r\nConnection: keep-alive"));
    m_ggclient->print(F("\r\nAccept-Encoding: gzip"));
    m_ggclient->print(F("\r\nAccess-Control-Allow-Credentials: true"));
    m_ggclient->print(F("\r\nAccess-Control-Allow-Origin: "));

    String outStr = F("http://");

#ifdef ESP8266
    outStr += WiFi.hostname().c_str();
#elif defined(ESP32)
    outStr += WiFi.getHostname();
#endif
    outStr += F(".local");
    m_ggclient->print(outStr);

    // Add content headers if body is not null
    if (strlen(body) > 0)
    {
        outStr = F("\r\nContent-Type: application/json\r\nContent-Length: ");
        outStr += strlen(body);
        m_ggclient->print(outStr);
    }
    if (bearer)
    {
        m_ggclient->print(F("\r\nAuthorization: Bearer "));
        m_ggclient->print(readParam("access_token"));
    }
    m_ggclient->print(F("\r\n\r\n"));
    if (strlen(body) > 0)
    {
        m_ggclient->print(body);
    }
}

bool GoogleOAuth2::parsePayload(const String &payload)
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
        log_error("%s", err.c_str());
        return false;
    }

    if (doc["expires_in"])
    {
        int secs = doc["expires_in"];
        m_expires_at_ms = (1000 * secs) + millis();
        log_info("%d seconds before token will expire.\n", secs);
    }

    if (doc["access_token"])
    {
        writeParam("access_token", doc["access_token"].as<String>().c_str());
    }

    if (doc["refresh_token"])
    {
        writeParam("refresh_token", doc["refresh_token"].as<String>().c_str());
        log_info("Refresh token: %s\n", doc["refresh_token"].as<String>().c_str());
    }

    if (doc["device_code"])
    {
        m_device_code = doc["device_code"].as<String>();
    }

    if (doc["user_code"])
    {
        m_user_code = doc["user_code"].as<String>();
    }

    if (doc["token_type"])
    {
        m_token_type = doc["token_type"].as<String>();
    }

    if (doc["error"])
    {
        log_error("Error: %s", doc["error"].as<String>().c_str());
        return false;
    }
    return true;
}

String GoogleOAuth2::gzipInflate(const String &compressedBytes) const
{
    String uncompressedBytes;

    unsigned int full = compressedBytes.length();
    unsigned int half = compressedBytes.length() / 2;

    unsigned int uncompLength = full;
    char *uncomp = (char *)calloc(sizeof(char), uncompLength);

    z_stream strm;
    strm.next_in = (Bytef *)compressedBytes.c_str();
    strm.avail_in = compressedBytes.length();
    strm.total_out = 0;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;

    bool done = false;

    if (inflateInit2(&strm, (16 + MAX_WBITS)) != Z_OK)
    {
        free(uncomp);
        return "";
    }

    while (!done)
    {
        // If our output buffer is too small
        if (strm.total_out >= uncompLength)
        {
            // Increase size of output buffer
            char *uncomp2 = (char *)calloc(sizeof(char), uncompLength + half);
            memcpy(uncomp2, uncomp, uncompLength);
            uncompLength += half;
            free(uncomp);
            uncomp = uncomp2;
        }

        strm.next_out = (Bytef *)(uncomp + strm.total_out);
        strm.avail_out = uncompLength - strm.total_out;

        // Inflate another chunk.
        int err = inflate(&strm, Z_SYNC_FLUSH);
        if (err == Z_STREAM_END)
            done = true;
        else if (err != Z_OK)
        {
            break;
        }
    }

    if (inflateEnd(&strm) != Z_OK)
    {
        free(uncomp);
        return "";
    }

    for (size_t i = 0; i < strm.total_out; ++i)
    {
        uncompressedBytes += uncomp[i];
    }
    free(uncomp);
    return uncompressedBytes;
}

bool GoogleOAuth2::readggClient(String &payload, bool keep_connection)
{
    uint16_t len = 0, pos = 0;

    // // Request sent to Google, wait for reply (with timeout)
    // for ( uint32_t timeout = millis(); (millis() - timeout > 1000) || m_ggclient->connected(); payload = m_ggclient->readStringUntil('\n')) {
    //     // Skip headers
    //     #if DEBUG_ENABLE
    //     Serial.println(payload);
    //     #endif
    //     if (payload == "\r") { break; }
    //     yield();
    // }

    // // Get body content
    // payload = "";

    // while (m_ggclient->available()) {
    //     payload += (char) m_ggclient->read();
    //     yield();
    // }

    // Add scope for local variable
    {
        String line;
        for (uint32_t timeout = millis();
             (millis() - timeout > 1000) || m_ggclient->connected();
             line = m_ggclient->readStringUntil('\n'))
        {
            if (line.indexOf("Content-Length:") > -1)
                len = line.substring(strlen("Content-Length: ")).toInt();
            if (line == "\r")
                break;
        }
    }

    // If there are incoming bytes available from the server, read them and store:
    payload = "";
    for (uint32_t timeout = millis(); (millis() - timeout > 1000) || pos < len;)
    {
        if (m_ggclient->available())
        {
            payload += (char)m_ggclient->read();
            pos++;
            yield();
        }
    }
    payload = gzipInflate(payload);

#if DEBUG_ENABLE
    Serial.println(payload);
#endif

    // Speed up some steps (for example during authentication procedure or renew of access token)
    if (!keep_connection)
        m_ggclient->stop();

    return parsePayload(payload);
}

void GoogleOAuth2::checkRefreshTime()
{
    // functionLog() ;
    long r_second = m_expires_at_ms - (long)millis();
    log_debug("%ld seconds before token will expire.", r_second / 1000);
    if (r_second < 10)
    {
        refreshToken();
    }
}

bool GoogleOAuth2::isAccessTokenValid()
{
    // functionLog() ;
    String outStr = readParam("access_token");
    if (outStr.length() < 128)
        return false;
    outStr = ("/tokeninfo?access_token=");
    outStr += readParam("access_token");

    sendCommand("GET ", AUTH_HOST, outStr.c_str(), "");

    String payload;
    payload.reserve(512);
    if (readggClient(payload))
        return true;
    return false;
}

bool GoogleOAuth2::refreshToken(bool stopClient)
{
    // functionLog() ;
    if (readParam("refresh_token").length() == 0)
        return false;

    String body = F("{\"grant_type\":\"refresh_token\",\"client_id\":\"");
    body += readParam("client_id");
    body += F("\",\"client_secret\":\"");
    body += readParam("client_secret");
    body += F("\",\"refresh_token\":\"");
    body += readParam("refresh_token");
    body += F("\"}");

    sendCommand("POST ", AUTH_HOST, "/token", body.c_str(), false);

    String payload;
    payload.reserve(512);
    if (!readggClient(payload, stopClient))
    {
        m_ggstate = INVALID;
        log_error("Token not refreshed!\n");
        return false;
    }
    return true;
}

bool GoogleOAuth2::requestDeviceCode()
{
    // functionLog() ;
    String body = F("{\"client_id\":\"");
    body += readParam("client_id");
    body += F("\",\"scope\":\"");
    body += readParam("scope");
    body += F("\"}");

    sendCommand("POST ", AUTH_HOST, "/device/code", body.c_str());

    String payload;
    payload.reserve(512);
    if (!readggClient(payload, true))
    {
        return false;
    }
    return true;
}

const char *GoogleOAuth2::getUserCode() const
{
    return m_user_code.c_str();
}

bool GoogleOAuth2::pollingAuthorize()
{
    // functionLog() ;

    // Check if user has submitted the code
    String body = F("{\"client_id\":\"");
    body += readParam("client_id");
    body += F("\",\"client_secret\":\"");
    body += readParam("client_secret");
    body += F("\",\"device_code\":\"");
    body += m_device_code; // readParam("device_code");
    body += F("\",\"grant_type\":\"urn:ietf:params:oauth:grant-type:device_code\"}");

    sendCommand("POST ", AUTH_HOST, "/token", body.c_str());

    String payload;
    payload.reserve(512);
    if (readggClient(payload, true))
    {
        if (m_token_type != "")
        {
            m_ggclient->stop();
            return true;
        }
    }

    m_ggclient->stop();
    return false;
}

String GoogleOAuth2::readParam(const char *keyword) const
{
    StaticJsonDocument<1024> doc;

    File file = m_filesystem->open(m_configFile, "r");
    if (file)
    {
        DeserializationError error = deserializeJson(doc, file);
        if (error)
        {
            Serial.print(F("Failed to deserialize file, may be corrupted: "));
            Serial.print(error.c_str());
            file.close();
            return "";
        }
        file.close();
        return doc[keyword].as<String>();
    }
    return "";
}

bool GoogleOAuth2::writeParam(const char *keyword, const char *value) const
{
    StaticJsonDocument<1024> doc;

    File file = m_filesystem->open(m_configFile, "r");
    if (file)
    {
        DeserializationError error = deserializeJson(doc, file);
        if (error)
        {
            Serial.print(F("Failed to deserialize file, may be corrupted: "));
            Serial.print(error.c_str());
            file.close();
            return false;
        }
        else
        {
            doc[keyword] = value;
            file = m_filesystem->open(m_configFile, "w");
            serializeJsonPretty(doc, file);
            file.close();
        }
        return true;
    }
    return false;
}

void GoogleOAuth2::clearConfig() const
{
    Serial.print(PSTR("Deleting file: \r\n"));
    Serial.println(m_configFile);
    if (m_filesystem->remove(m_configFile))
        Serial.println(PSTR("- file deleted"));
    else
        Serial.println(PSTR("- delete failed"));
}

// Prints the content of a file to the Serial
// (use of ArduinoJson for a pretty output)
void GoogleOAuth2::printConfig() const
{
    // Open file for reading
    Serial.println(PSTR("\nActual config file: "));
    File file = m_filesystem->open(m_configFile, "r");
    while (file.available())
    {
        Serial.print((char)file.read());
    }
    Serial.println();
    file.close();
}
