#include "GoogleOAuth2.h"

// external parser callback, just calls the internal one
void __JSONCallback(uint8_t filter, uint8_t level, const char *name, const char *value, void *cbObj)
{
	// call GoogleOAuth2 internal handler
	((GoogleOAuth2 *)cbObj)->parseJson(filter, level, name, value);
}


// External function for custom json parse (will receive class pointer as parameter)
void GoogleOAuth2::parseJson(uint8_t filter, uint8_t level, const char *name, const char *value)
{
    size_t len = strlen(value);
    if(strstr(name, "expires_in")) 
    {
        long _time = strtol(value, NULL, 10);
        expires_at_ms = 1000 * _time + millis();  
        Serial.print(_time);
        Serial.println(" seconds before token will expire.");
        _parser.setOutput(value, len);
    }

    if(strstr(name, "access_token")) 
    {               
        if(strcmp( _parser.filter.keyword, "noWrite") != 0)
            writeParam("access_token", value);
        _parser.setOutput(value, len);
    }

    if(strstr(name, "refresh_token")) 
    {              
        if(strcmp( _parser.filter.keyword, "noWrite") != 0)
            writeParam("refresh_token", value);
        _parser.setOutput(value, len);
    }

    if(strstr(name, "user_code")) 
    {       
        char buffer[12];
        PString code(buffer, sizeof(buffer));
        if(len < 11){
            code = value;
            for(uint8_t i=0; i< 12-len; i++)
                code += ' ';
        }
        else
            code = value;
        
        if(strcmp( _parser.filter.keyword, "noWrite") != 0)       
            writeParam("user_code", code);
        _parser.setOutput(code, strlen(code));
    }

    if(strstr(name, "device_code")) 
    {      
        if(strcmp( _parser.filter.keyword, "noWrite") != 0)        
            writeParam("device_code", value);
        _parser.setOutput(value, len);
    }

    if(strstr(name, "verification_url")) 
    {   
        if(strcmp( _parser.filter.keyword, "noWrite") != 0)          
            writeParam("verification_url", value);
        _parser.setOutput(value, len);
    }

    if(!strcmp(name, "error")){
        Serial.print("\nError: ");
        Serial.println(value);
        _parser.setOutput("error", strlen("error"));
    }

}


GoogleOAuth2::GoogleOAuth2(fs::FS *fs) 
{
    functionLog() ;
    _filesystem = fs;
    _parser.setCallback(__JSONCallback, this);
    _ggstate = INIT;
}


GoogleOAuth2::GoogleOAuth2(fs::FS *fs, const char *configFile)
{
    functionLog() ;
    _filesystem = fs;
    _configFile = configFile;
    _parser.setCallback(__JSONCallback, this);
    _ggstate = INIT;
}


bool GoogleOAuth2::begin(){
    File file = _filesystem->open(_configFile, "r");    
    if(!file || file.isDirectory()){       
        Serial.print(F("Failed to read configuration file"));
        Serial.println(_configFile);
        Serial.println(F("If you call begin() without parameter, a configuration file must be present in filesystem"));
        return false;
    }
    const char *id = readParam("client_id");
    const char *secret = readParam("client_secret");
    const char *_scope =readParam("scope");
    return begin(id, secret, _scope);
}


bool GoogleOAuth2::begin(const char *id, const char *secret, const char *_scope)
{
    functionLog() ;
    expires_at_ms = 30000;
	
#ifdef ESP8266 
    ggclient.setInsecure();
    ggclient.setBufferSizes(1024,1024);
#endif

    // Load confif file or create it if don't exist
    if (loadConfig(id, secret, _scope)){

		if (isAccessTokenValid())
		{
			_ggstate = GOT_TOKEN;
			Serial.println(PSTR("\nAccess token valid"));
		}
		else if (refreshToken())
			{
				Serial.println(PSTR("\nAccess token refreshed"));
				_ggstate = GOT_TOKEN;
			}
	} else 
    {
        return false;
    }

    if( _ggstate != GOT_TOKEN){
        // No valid access token (is this the first time we authorize app?)
        requestDeviceCode();
        Serial.println(PSTR("\nNo valid access token founded. "));
        _ggstate = REQUEST_AUTH;
    }
     return true;
}


bool GoogleOAuth2::loadConfig(const char *id, const char *secret, const char *_scope)
{
    functionLog() ;
    if(! _filesystem->exists(_configFile)){
        File newfile = _filesystem->open(_configFile, "w");   
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

    File file = _filesystem->open(_configFile, "r");
    if (!file)
    {
        Serial.println(F("Failed to open file"));
        return false;
    }

    // Init json parser
	_parser.reset();
    readggClient( __func__ , "noWrite");
    while(file.available()){
        _parser.feed(file.read());
    }    

	return true;
}

int GoogleOAuth2::getState()
{
    functionLog() ;
    if( _ggstate == GOT_TOKEN)
        return _ggstate;

    // No valid access token found, wait for user authorization
    if(_ggstate == REQUEST_AUTH){
        if (pollingAuthorize())
        {
            _ggstate = GOT_TOKEN;
        }
    }
    return _ggstate;
}


bool GoogleOAuth2::doConnection( const char *host ){
    functionLog() ;
    
    ggclient.setNoDelay(true);
    ggclient.stop();
    serialLog(PSTR("\nStarting connection to server..."));
    if (!ggclient.connect(host, PORT)) {
        serialLogln(PSTR(" failed!"));
        return false;
    }
    else
        serialLogln(PSTR(" Done!"));
    return true;
}


const char* GoogleOAuth2::sendCommand(const char *const &rest, const char *const &host, const char *const &command,
                                      const char *const &body, bool bearer = false)
{
    functionLog() ;

    if (!ggclient.connected()) // || (millis() - lastConnectTime > SERVER_TIMEOUT))
        doConnection(host);

    // token is still valid for less than 10s
    long r_second = expires_at_ms - (long) millis();
    serialLogln();
    serialLog(r_second/1000);
    serialLogln(" seconds before token will expire.");

    if(r_second < 10000 && (strcmp(command, "/token") != 0)) 
    {
        serialLogln("Call refreshToken() from sendCommand()");
		serialLogln(command);
        delay(500);
        expires_at_ms += 30000; // times for refreshing
		refreshToken();
    }

#if DEBUG_MODE
    Serial.printf("\nINFO - HTTP request: %s%s HTTP/1.1\n", rest, command);
#endif

    ggclient.print(rest);
    ggclient.print(command);
    ggclient.print(" HTTP/1.1");
    ggclient.print("\r\nHost: ");
	ggclient.print(host);
    ggclient.print("\r\nUser-Agent: ESP32");
    ggclient.print("\r\nConnection: keep-alive");
    ggclient.print("\r\nAccess-Control-Allow-Credentials: true");
    ggclient.print("\r\nAccess-Control-Allow-Origin: "); 
 #ifdef ESP8266
    const char* hostname =  WiFi.hostname().c_str();  
 #elif defined(ESP32) 
    const char* hostname =  WiFi.getHostname();
 #endif
    char* const _localhost = new char[strlen(hostname) + 14];
    snprintf_P(_localhost, strlen(_localhost), PSTR("http://%s.local"), hostname);
    ggclient.print(_localhost);

    // Add content headers if body is not null
    if(strlen(body)> 0)
    {
        size_t len = strlen_P(PSTR("\r\nContent-Type: application/json\r\nContent-Length: ")) + 5;  //max 99999bytes
        char* headers = new char[len];
        snprintf_P(headers, len, PSTR("\r\nContent-Type: application/json\r\nContent-Length: %d"), strlen(body));
        ggclient.print(headers);
        serialLogln(headers);
    }
    if (bearer) 
    {        
        ggclient.print("\r\nAuthorization: Bearer ");
        ggclient.print(readParam("access_token"));
    }
    ggclient.print("\r\n\r\n");
	if(strlen(body)> 0) 
    {   
		ggclient.print(body);
        serialLogln(body);
    }
    return nullptr;
}


void GoogleOAuth2::readggClient(const char* funcName, const char* key, bool keep_connection)
{
    functionLog() ;
    // Init HTTP stream parser
	_parser.reset();
    _parser.setSearchKey(funcName, key);

    // Skip headers
    while (ggclient.connected()) {
        static char old;
        char ch = ggclient.read();
        if (ch == '\n' && old == '\r') {
            break;
        }
        old = ch;
    }
    // get body content
    while (ggclient.available()) {
        char c = ggclient.read();
        _parser.feed(c);
        serialLog((char)c);
        yield();
    }

    // Speed up some steps (for example during authentication procedure or renew of access tokem)
    // Otherwise close connection and release memory 
    if(!keep_connection)
        ggclient.stop();
}


bool GoogleOAuth2::isAccessTokenValid()
{
    functionLog() ;
    const char* access_token = readParam("access_token");
    size_t len = strlen_P(PSTR("/tokeninfo?access_token=")) + strlen(access_token);
    char* cmd = new char[len + 1];
	snprintf_P(cmd, len+1, PSTR("/tokeninfo?access_token=%s"), access_token);

    sendCommand("GET ", AUTH_HOST, cmd, "");
    readggClient( __func__ , "access_token", true);
    const char * res = _parser.getOutput();    
    
    if (res == nullptr)
        return false;
    
    if(strstr(res, "error"))
        return false;
    
    ggclient.stop();
    return true;
}


bool GoogleOAuth2::refreshToken()
{
    functionLog() ;
    char buffer[350];
    PString body(buffer, sizeof(buffer));

    if(strlen(readParam("refresh_token")) == 0)
        return false;
    
    body =  F("{\"grant_type\":\"refresh_token\",\"client_id\":\"");
    body += readParam("client_id");
    body += F("\",\"client_secret\":\"");
    body += readParam("client_secret");
    body += F("\",\"refresh_token\":\"");
    body += readParam("refresh_token");
    body += F("\"}");    

    sendCommand("POST ", AUTH_HOST, "/token", body, false);
    readggClient( __func__ , "access_token", true);
    //const char * res = _parser.getOutput();
	const char* res = "ciao!";
    if(strstr(res, "error"))
        return false;

    ggclient.stop();
    return true;
}


bool GoogleOAuth2::requestDeviceCode()
{
    functionLog() ;

    char buffer[200];
    PString body(buffer, sizeof(buffer));

    body =  F("{\"client_id\":\"");
    body += readParam("client_id");
    body += F("\",\"scope\":\"");
    body += readParam("scope");
    body += F("\"}");

    sendCommand("POST ", AUTH_HOST, "/device/code", body);    
    readggClient( __func__ , "device_code", true);
    const char * res = _parser.getOutput();
    if(strstr(res, "error"))
        return false;
    return true;
}


bool GoogleOAuth2::pollingAuthorize()
{
    functionLog() ;
    // Inform the user about pending authorization request
    Serial.println(PSTR("\nApplication need to be authorized!"));
    Serial.print(PSTR("Open with a browser the address < "));
    Serial.print(readParam("verification_url"));
    Serial.print(PSTR(" > and insert this confirmation code "));
    Serial.println(readParam("user_code"));

    // Check if user has submitted the code    
    char buffer[400];
    PString body(buffer, sizeof(buffer));

    body =  F("{\"client_id\":\"");
    body += readParam("client_id");
    body += F("\",\"client_secret\":\"");
    body += readParam("client_secret");
    body += F("\",\"device_code\":\"");
    body += readParam("device_code");
    body += F("\",\"grant_type\":\"urn:ietf:params:oauth:grant-type:device_code\"}");

    sendCommand("POST ", AUTH_HOST, "/token", body);
    readggClient( __func__ , "polling");
    const char * res = _parser.getOutput();
    if(strstr(res, "error"))
        return false;
        
    ggclient.stop();
    return true;   
}

// Read a specific parameter from comnfig file
const char * GoogleOAuth2::readParam(const char * keyword){
    //functionLog() ;
    #define BUF_LEN 256
    char * buffer = new char[BUF_LEN+1];

    File file = _filesystem->open(_configFile, "r");    
    if(!file || file.isDirectory()){       
        Serial.print(F("Failed to read file"));
        Serial.println(_configFile);
        return "";
    }
   
    while(file.available()){
        if (file.find(keyword)){
            // "access_token": "nxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
            // Skip 4 chars  ": "            
            file.read(); file.read();file.read(); file.read(); 
            unsigned int i = 0;
            while(file.available()){                
                char ch = file.read();                
                if(ch == '\"'){
                  buffer[i] = '\0' ;
                  break;
                }
                if(i<BUF_LEN) {
                    buffer[i++] = ch;   // add char to buffer
                }
                else {
                    file.close(); 
                    return "";          // buffer too small 
                }       
            }            
            file.close();       
            return buffer;
        }           
    }
    file.close();
    return "";
}

// value must be WITH THE SAME size of replaced text;
bool GoogleOAuth2::writeParam(const char * keyword, const char * value){
    functionLog() ;

    // Open file for writing
    File file = _filesystem->open(_configFile, "r+"); 
    if(!file || file.isDirectory()){       
        Serial.print(F("Failed to open file "));
        Serial.println(_configFile);
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
    file.print(",\r\n  \"");
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
    Serial.println(_configFile);
    if (_filesystem->remove(_configFile))
        Serial.println(PSTR("- file deleted"));
    else
        Serial.println(PSTR("- delete failed"));
}

// Prints the content of a file to the Serial
// (use of ArduinoJson for a pretty output)
void GoogleOAuth2::printConfig()
{
    // Open file for reading
    Serial.println(PSTR("\nActual config file: "));
    File file = _filesystem->open(_configFile, "r");
    while(file.available()){
       Serial.print((char)file.read());
    }
    Serial.println();
    file.close();
}
