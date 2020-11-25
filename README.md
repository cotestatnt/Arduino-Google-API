# Arduino-Google-API

With **GoogleAPI Arduino library**, you can easily authenticate your device as OAuth 2.0 client without user interaction except for the first time when your application must be authorized in order to provide the user's consent.

Note that you will be warned about "unverified App". Just click on *advanced* and then go to *---your app name---(unsafe)*.
(If you need, you can send a verification request to Google)

The first step is enable all the required APIs (Drive, Gmail, Calendar etc etc), then you can edit or create a new Project and set-up credentials in order to abtain a valid **Client ID, Client Secret, API key**. You have to define also wich [scope](https://developers.google.com/identity/protocols/oauth2/scopes) your application will be able to manage.
Follow instructions provided in [API Console Help](https://support.google.com/googleapi/answer/7037264) for detailed instructions and How-To.
To create your project and credentials visit the page [Google API Console](https://console.developers.google.com/apis/credentials)

![Google API credentials](/credentials1.png)

Then user have to authorize the application. There are two ways to do it with this library:
* Authorize your application as TV and Limited Input using OAuth 2.0 endpoint for TV and Limited-Input Device
* Authorize your application as Web application using OAuth 2.0 endpoint for Web Server Applications (for this method you need also API key)

With the first choice, the user switches to a device with richer input capabilities, launches a web browser, navigates to the URL displayed and enters the code provided. The user can then grant (or deny) access to your application.
This method allow only few scopes for your application.

The second choice, can be used for every scope you need, but a small webserver running in our device is necessary because we have to call Google APIs for tokens directly from device!
In the examples provided with library, you will find a webserver that can load pages from flash memory (ESP32 or ESP8266) or SD (To Do!).
For convenience, the built-in *token.html* page can handle all the flow for you and also save the json configuration file. You can call it passing as argument the configuration file to load or without argument and write all necessary inputs after.
Ex: http://espfs.local/token.htm?json=gmail.json or http://espfs.local/token.htm?json=gdrive.json and so on.

![Tokens Helper page](/token_helper.png)

N.B.
Be carefull, this library needs a filesystem in order to store all necessary data, tokens and webserver pages.
When you choice the board, set properly the partition scheme if necessary. Ex: for ESP8266 the best choice for me is LittleFS and you can use the convenient [Arduino IDE tool Arduino ESP8266 LittleFS Filesystem Uploader](https://github.com/earlephilhower/arduino-esp8266littlefs-plugin) in order to upload html webserver files alls in once.

With ESP32 the best filesystem for me is FFat (Fast FAT), but I was not able to find a working equivalent tool.
You can still easily upload all necessary files one by one with the library built-in editor http://espfs.local/edit or use SPIFFS (slower and without folder support). With SD card you can copy manually the files.
