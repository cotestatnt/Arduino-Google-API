#include "GoogleOAuth2.h"

static const char digicert[] PROGMEM = R"EOF(
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

GoogleOAuth2::GoogleOAuth2(fs::FS *fs) 
{
    functionLog() ;
    m_filesystem = fs;
    m_ggstate = INIT;
}


GoogleOAuth2::GoogleOAuth2(fs::FS *fs, const char *configFile)
{
    functionLog() ;
    m_filesystem = fs;
    m_configFile = configFile;
    m_ggstate = INIT;
}


bool GoogleOAuth2::begin(){
    File file = m_filesystem->open(m_configFile, "r");    
    if(!file || file.isDirectory()){       
        Serial.print(F("Failed to read configuration file"));
        Serial.println(m_configFile);
        Serial.println(F("If you call begin() without parameter, a configuration file must be present in filesystem"));
        return false;
    }
    const char *id = readParam("client_id").c_str();
    const char *secret = readParam("client_secret").c_str();
    const char *_scope =readParam("scope").c_str();
    return begin(id, secret, _scope);
}


bool GoogleOAuth2::begin(const char *id, const char *secret, const char *_scope)
{
    functionLog() ;
    m_expires_at_ms = 30000;

#ifdef ESP8266 
    //m_ggclient.setInsecure();

    // Load root certificate from file is exists
    if(m_filesystem->exists("/ca.cer") ){
        Serial.println(F("Root cerfificate loaded from file /ca.cer"));
        File file_cert = m_filesystem->open("/ca.cer", "r");
        m_ggclient.loadCACert(file_cert);
    } else {
        m_ggclient.setCACert_P(digicert, sizeof(digicert));
    }

    m_ggclient.setSession(&m_session);
    m_ggclient.setBufferSizes(TCP_MSS, TCP_MSS);
#endif

    // Load confif file or create it if don't exist
    if (loadConfig(id, secret, _scope))
    {
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

    if( m_ggstate != GOT_TOKEN)
    {
        // No valid access token (is this the first time we authorize app?)        
        Serial.println(PSTR("\nNo valid access or refresh token found in config file."));		
		if (requestDeviceCode()){
            m_ggstate = REQUEST_AUTH; 
            return true;
        } 
        else {
            m_ggstate = INVALID;
            return false;
        }                
    }
    // default value
    return false;
}


bool GoogleOAuth2::loadConfig(const char *id, const char *secret, const char *_scope)
{
    functionLog() ;
    if(! m_filesystem->exists(m_configFile))
    {
        File newfile = m_filesystem->open(m_configFile, "w");   
        if (!newfile)
        {
            Serial.println(F("Failed to crete config file"));
            return false;
        }
        newfile.print(F("{\r\n"));      
        newfile.print(F("  \"scope\": \""));         newfile.print(_scope); newfile.print(F("\",\r\n"));  
        newfile.print(F("  \"client_id\": \""));     newfile.print(id);     newfile.print(F("\",\r\n"));  
        newfile.print(F("  \"client_secret\": \"")); newfile.print(secret); newfile.print(F("\"\r\n}"));  
        newfile.close();        
        Serial.println(F("Config file created."));
        return true;
    }

    File file = m_filesystem->open(m_configFile, "r");
    if (!file)
    {
        Serial.println(F("Failed to open file"));
        return false;
    }

	return true;
}

int GoogleOAuth2::getState()
{
    functionLog() ;
    if (m_ggstate == REQUEST_AUTH && pollingAuthorize()){
        m_ggstate = GOT_TOKEN; 
        Serial.println("Got token");
    }
    return m_ggstate;
}


bool GoogleOAuth2::doConnection( const char *host ){
    functionLog() ;
    /*
    if(m_ggclient.connected()){
        m_ggclient.stop();
    }
    */
    serialLog(PSTR("\nStarting connection to server..."));

    if (!m_ggclient.connect(host, PORT)) 
    {
        serialLogln(PSTR(" failed!"));
        return false;
    }
    else
        serialLogln(PSTR(" Done!"));
    return true;
}


void GoogleOAuth2::sendCommand(const char *const &rest, const char *const &host, const char *const &command,
                                      const char *const &body, bool bearer = false)
{
    functionLog() ;

    // Refresh token if necessary except if this is called from refreshToken()
    if(strcmp(command, "/token") != 0) 
        checkRefreshTime();

#if DEBUG_MODE
    Serial.printf("\nINFO - HTTP request: %s%s HTTP/1.1\n", rest, command);
#endif

    if (!m_ggclient.connected()) // || (millis() - lastConnectTime > SERVER_TIMEOUT))
        if (!doConnection(host))
            return ;
    m_ggclient.print(rest);
    m_ggclient.print(command);
    m_ggclient.print(F(" HTTP/1.1"));
    m_ggclient.print(F("\r\nHost: "));
    m_ggclient.print(host);
    //m_ggclient.print(F("\r\nUser-Agent: arduino-esp"));
    //m_ggclient.print(F("\r\nConnection: keep-alive"));
    m_ggclient.print(F("\r\nAccess-Control-Allow-Credentials: true"));
    m_ggclient.print(F("\r\nAccess-Control-Allow-Origin: ")); 

    char buffer[64];
    PString outStr(buffer, sizeof(buffer));
    outStr = F("http://");

#ifdef ESP8266
    outStr += WiFi.hostname().c_str();  
#elif defined(ESP32) 
    outStr += WiFi.getHostname();
#endif
    outStr += F(".local");
    m_ggclient.print(outStr);

    // Add content headers if body is not null
    if(strlen(body)> 0)
    {   
       outStr = F("\r\nContent-Type: application/json\r\nContent-Length: ");
       outStr += strlen(body);
       m_ggclient.print(outStr);
    }
    if (bearer) 
    {        
        m_ggclient.print(F("\r\nAuthorization: Bearer "));
        m_ggclient.print(readParam("access_token"));
    }
    m_ggclient.print(F("\r\n\r\n"));
    if(strlen(body)> 0) 
    {   
        m_ggclient.print(body);
        serialLogln(body);
    }
    
}


String GoogleOAuth2::parseLine(String &line, const char *filter = nullptr) 
{

    /*
    * The access token check request return expires_in seconds or error if not valid
    * The refresh token request, will give the new access token
    * The device code request, will give device_code and user_code
    * The polling authorize request will give back access_token, refresh_token and expires_in
    */

    String value = "";
    value.reserve(200);             // The max length for searched string is 200 (access_token)

    value = getValue(line, "\"expires_in\": \"" );
    if(value.length())  { 
        long _time = value.toInt();
        m_expires_at_ms = 1000 * _time + millis();  
        Serial.print(_time);
        Serial.println(" seconds before token will expire.");
    }

    value = getValue(line, "\"access_token\": \"" );
    if(value.length()) { 
        writeParam("access_token", value.c_str());
        m_expires_at_ms = 3599000L; 
    }

    value = getValue(line, "\"refresh_token\": \"" );
    if(value.length()) { 
        writeParam("refresh_token", value.c_str());
    }

    value = getValue(line, "\"device_code\": \"" );
    if(value.length()) {  
        m_device_code = (char*)realloc( m_device_code, sizeof(char *) * (value.length()+1) );
        strcpy( m_device_code , value.c_str());       
    }

    value = getValue(line, "\"user_code\": \"" );
    if(value.length()) {  
        m_user_code = (char*)realloc( m_user_code, sizeof(char *) * (value.length()+1) );
        strcpy( m_user_code , value.c_str());       
        return m_user_code;
    }

    value = getValue(line, "\"token_type\": \"" );
    if(value.length()) {       
        return value;
    }

    value = getValue(line, "\"error\": \"" );
    if(value.length()) {  
        String err = "error: ";
        err += value;       
        return err;
    } 

    return "";
}


const char* GoogleOAuth2::readggClient( bool keep_connection)
{
    functionLog() ;

    // Skip headers
    while (m_ggclient.connected()) {
        String line = m_ggclient.readStringUntil('\n');
        if (line == "\r") {
        // serialLogln(line);
        break;
        }
    }
    // get body content
    String res;
    res.reserve(200);
    while (m_ggclient.available()) { 
        String line = m_ggclient.readStringUntil('\n');
        serialLogln(line);
        res = parseLine( line);
        
        // value found in json response (skip all the remaining bytes)
        if(res.length() > 0){
            while (m_ggclient.available()) { 
                m_ggclient.read(); 
                yield();
            }
            m_ggclient.stop();
            return res.c_str();
        }
        
    }    

    // Speed up some steps (for example during authentication procedure or renew of access tokem)
    // Otherwise close connection and release memory 
    if(!keep_connection)
        m_ggclient.stop();
    return res.c_str();
}


void GoogleOAuth2::checkRefreshTime(){
    //functionLog() ;
    long r_second = m_expires_at_ms - (long) millis();
    serialLogln();
    serialLog(r_second/1000);
    serialLogln(" seconds before token will expire.");
    if(r_second < 10 ) {
        refreshToken();
    }
}

bool GoogleOAuth2::isAccessTokenValid()
{
    functionLog() ;
    String outStr = readParam("access_token");
    if(outStr.length() < 128)
        return false;
    outStr = ("/tokeninfo?access_token=");    
    outStr += readParam("access_token");
    
    sendCommand("GET ", AUTH_HOST, outStr.c_str(), "");
    String res = readggClient(true);	
    if (res.indexOf("error") > -1){
        Serial.println(res);
        return false;
    }
    return true;
}


bool GoogleOAuth2::refreshToken( bool stopClient)
{
    functionLog() ;
    if(readParam("refresh_token").length() == 0)
        return false;
    
    String body =  F("{\"grant_type\":\"refresh_token\",\"client_id\":\"");
    body += readParam("client_id");
    body += F("\",\"client_secret\":\"");
    body += readParam("client_secret");
    body += F("\",\"refresh_token\":\"");
    body += readParam("refresh_token");
    body += F("\"}");    

    sendCommand("POST ", AUTH_HOST, "/token", body.c_str(), false);
    String res = readggClient(stopClient);	
    if (res.indexOf("error") > -1){
        Serial.println(res);
        m_ggstate = INVALID;
        return false;
    }
    return true;
}


bool GoogleOAuth2::requestDeviceCode()
{
    functionLog() ;
    String body =  F("{\"client_id\":\"");
    body += readParam("client_id");
    body += F("\",\"scope\":\"");
    body += readParam("scope");
    body += F("\"}");

    sendCommand("POST ", AUTH_HOST, "/device/code", body.c_str());    
    String res = readggClient(true);	
    if (res.indexOf(F("error")) > -1){
        Serial.println(res);
        return false;
    }
    return true;
}


const char* GoogleOAuth2::getUserCode(){
    return m_user_code;
}


bool GoogleOAuth2::pollingAuthorize()
{
    functionLog() ;

    // Check if user has submitted the code    
    String body =  F("{\"client_id\":\"");
    body += readParam("client_id");
    body += F("\",\"client_secret\":\"");
    body += readParam("client_secret");
    body += F("\",\"device_code\":\"");
    body += m_device_code; //readParam("device_code");
    body += F("\",\"grant_type\":\"urn:ietf:params:oauth:grant-type:device_code\"}");

    sendCommand("POST ", AUTH_HOST, "/token", body.c_str());

    String res = readggClient(true);
    if (res.indexOf("Bearer") > -1){   
        Serial.print("Token type ");   
        Serial.println(res); 
        m_ggclient.stop(); 
        return true;
    }
        
    m_ggclient.stop();
    return false;   
}

// Read a specific parameter from comnfig file
const String GoogleOAuth2::readParam(const char * keyword) const{
    //functionLog() ;
    String outStr = "";
    outStr.reserve(256);

    File file = m_filesystem->open(m_configFile, "r");    
    if(!file || file.isDirectory()){       
        Serial.print(F("Failed to read file"));
        Serial.println(m_configFile);
        return "";
    }
   
    while(file.available()){
        if (file.find(keyword)){
            // "access_token": "nxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            // Skip 4 chars  ": "            
            file.read(); file.read();file.read(); file.read(); 
            if(file.available()){                
                outStr = file.readStringUntil('\"');                
            }            
            file.close();     
            return outStr;
        }  
        return "";         
    }
    file.close();
    return "";
}

// value must be WITH THE SAME size of replaced text;
bool GoogleOAuth2::writeParam(const char * keyword, const char * value){
    functionLog() ;

    // Open file for writing
    File file = m_filesystem->open(m_configFile, "r+"); 
    if(!file || file.isDirectory()){       
        Serial.print(F("Failed to open file "));
        Serial.println(m_configFile);
        return false;
    }

    while(file.available()){
        if (file.find(keyword)){
            // "access_token": "nxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            // Skip 4 chars  ": "            
            size_t pos = file.position() + 4;
            file.seek(pos);
            file.print(value);
            file.print("\"");
            file.close();        
            return true;
        }           
    }

    // Append param at the end of file if not found
    file.seek(file.size() - 3);  // "\r\n}"
    file.print(" , \r\n  \"");
    file.print(keyword);
    file.print("\": \"");
    file.print(value);
    file.print("\"\r\n}");       
    file.close();        
    return true;
}


void GoogleOAuth2::clearConfig()
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
    while(file.available()){
       Serial.print((char)file.read());
    }
    Serial.println();
    file.close();
}


const String GoogleOAuth2::getValue(String &line, const char* param) const
{
    String value;
    size_t pos = line.indexOf(param);            
    if(pos != std::string::npos){
        pos += strlen_P(param) ;
        size_t valLen ;
        if(line.indexOf("\"", pos) > -1)
            valLen = line.indexOf("\"", pos) - pos;
        else
            valLen = line.indexOf(",", pos) - pos;
        value = line.substring(pos, pos+valLen);   
    }
    return value;
}
