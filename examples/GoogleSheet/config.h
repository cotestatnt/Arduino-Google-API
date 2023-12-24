// Timezone definition to get properly time from NTP server
#define MYTZ "CET-1CEST,M3.5.0,M10.5.0/3"
#define FILESYSTEM LittleFS

#define HOSTNAME     "esp2sheet"     // Setting hostname, you can access to http://HOSTNAME.local instead fixed ip address
#define FOLDER_NAME  "SheetFolder"
#define S_FILENAME   "myTestSheet"
#define SHEETNAME    "datasource"


// Google API OAuth2.0 client setup default values (you can change later with setup webpage)
const char* client_id     =  "xxxxxxxxxxxx-xxxxxxxxxxxxxxxxxxxxxx.apps.googleusercontent.com";
const char* client_secret =  "xxxxxx-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
const char* api_key       =  "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
const char* scopes        =  "https://www.googleapis.com/auth/drive.file "
                             "https://www.googleapis.com/auth/spreadsheets ";
							 
/* 
* You can use this redirect_uri if you don't have your own.
* This is a node.js server side application running on Heroku platform
* With Heroku CLI you can clone the repo https://git.heroku.com/arduino-google-api.git
* in order to be able to edit your own custom redirect page.
*/
const char* redirect_uri  =  "https://arduino-google-api.herokuapp.com";

static const char button_html[] PROGMEM = R"EOF(
<button class='button' onclick=window.open('/restart')>Restart ESP</button><button class='button' style= 'background-color: crimson;' onclick=window.open('/config')>Authorize ESP</button><hr>
)EOF";

static const char info_html[] PROGMEM = R"EOF(
<div style='text-align:center'>Save options if needed, then insert WiFi credentials and connect to SSID.<br>
Once connected to internet, your device need to be authorized from Google in order to write Sheets.<hr>
If you are still connected to local ESP Access Point, disconnect and reload webpage <br><br> <a href='http://SETUP_PAGE__PLACEHOLDER.local'>http://SETUP_PAGE__PLACEHOLDER.local</a><br><br>
then click on button <b>Authorize ESP</b> to start Google OAuth2.0 authentication workflow</div><hr>
)EOF";
