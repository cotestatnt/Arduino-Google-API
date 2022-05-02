# Arduino-Google-API

** STILL IN DEVELOPMENT ** Proveded as is

# YouTube video tutorial
[Arduino Google API, create and prepare a new Google Cloud Console project](https://youtu.be/YckdKgxf4bI)

[Arduino Google API, upload and configure project with ESP device](https://www.youtube.com/watch?v=VSEguvlvjnY)


### Introduction
With **GoogleAPI Arduino library**, you can easily authenticate your Espressif (ESP8266 and ESP32) device as OAuth 2.0 client without user interaction except for the first time when your application must be authorized in order to provide the user's consent.

The first step is create a new Project in the **Google API console** and set-up credentials in order to abtain a valid **Client ID, Client Secret, API key** and enable all the required APIs (Drive, Gmail, Sheets, Calendar etc etc). 

You have to define also wich [scope](https://developers.google.com/identity/protocols/oauth2/scopes) your application will be able to manage.
Follow instructions provided in [API Console Help](https://support.google.com/googleapi/answer/7037264) for detailed instructions and How-To.
To create your project and credentials visit the page [Google API Console](https://console.developers.google.com/apis/credentials)

![Google API credentials](/credentials1.png)

Client ID, Client Secret, API key and Scope can be created/defined **only once from the developer**, then user have to authorize the application in order to get a valid **access token** and a **refresh token** to renew the access token when expired.

The easiest way to use Google's APIs would be using Javascript OAuth 2.0 for Client-side library provided from Google itself in webpage hosted on ESP device, but unfortunately only top-level domain can be white-listed as "Authorized JavaScript origins" so this way is impracticable for our purposes.

This library will authorize your device as **Web Service** using OAuth 2.0 endpoint for [Web Server Applications](https://developers.google.com/identity/protocols/oauth2/web-server) and a little webserver hosted on ESP is necessary.
For your (or users) convenience, the provided **http://(your_device_address)/setup** page can handle all necessary steps in background and finally save the json configuration file in the ESP memory.
Old version of ESP32 core for Arduino could have a little bug to fix https://github.com/espressif/arduino-esp32/issues/3652

Note that users will be warned about "unverified App" unless you submit to Google a request for verification, if you require. 
You can simply skip for testing purpose or if you don't need to redistribute: just click on *advanced* and then go to *---your app name---(unsafe)*.

## Important - Redirect URI

As you can read in the Google documentation, for this authorization scenario it is necessary un authorized redirect URI as endpoints to which the OAuth 2.0 server can send the generated tokens.
If you own a top-level host, it's possible write a small php script on your webspace and use it as redirect uri.

Another good option for redirect uri, is using a micro-service like [pipedream.com](https://pipedream.com/) for example. 
The free plan will be largely sufficient for most cases (remember, you will need only once time for user). This is my pipedream [working workflow](https://pipedream.com/@cotestatnt/smart-air-p_mkCk3JW), feel free to copy and use for your convenience.
The redirect uri to be used will be the one generated when you deploy your workflow (something like xxxxxxxxxx.m.pipedream.net).

A last (and my preferred option) is use a cloud service platform like https://www.heroku.com/
With a free plan, you can run up to 2 process and it's more of enought for our purpose. You can use redirect link provided in examples, but if you need to create your specific project (custom text or logos for example), cloning this repo (with Heroku CLI) can be a good starting point: https://git.heroku.com/arduino-google-api.git

The redirect uri has to be inserted in your OAuth 2.0 Client ID credential definition (edit it after create). Remember also to add https://apis.google.com as authorized Javascript origin.

![OAuth 2.0 Client ID](/credentials2.png)

### Features
+ Authorize application for the allowed scopes
+ Save and refresh access token (local filesystem )

GoogleSheetAPI Class 
+ Create spreadsheet on Google Drive
+ Create sheets in a provided spreadsheet
+ Append rows to spreadsheet (formula supported)
 
GoogleDriveAPI Class 
+ Create folders on Google Drive
+ Upload files on Google Drive
+ Update existing files on Google Drive
+ Search for a specific file or folder

GoogleGmailAPI Class
+ Get list of unread messages
+ Read metadata of message
+ Read message body (snippet or full text)
+ Send message to specific address

### To do
+ Add support for Calendar API
+ Download files (Google Drive API)
+ Set metadata for message (Gmail API)

### Known bugs
+ With ESP8266 the **Connect to SSID** button from config page will timeout even if device it's connected correctly. Ignore and reload the page.

### Supported boards
The library works with the ESP8266 and ESP32 chipset.

+ 1.0.5   Web server handled by https://github.com/cotestatnt/esp-fs-webserver library. Redesign of configuration page
+ 1.0.4   Webserver included in source (no need to load Data folder in filesystem).
+ 1.0.3   Added support for Google Mail API
+ 1.0.2   Added support for Google Sheet API
+ 1.0.1   Added support for Google Drive API
+ 1.0.0   Initial version

