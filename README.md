# Arduino-Google-API

** STILL IN DEVELOPMENT ** Provided as is

### Introduction

With the **GoogleAPI Arduino library**, you can easily authenticate your Espressif (ESP8266 and ESP32) device as an OAuth 2.0 client without user interaction, except for the first time when your application must be authorized to provide the user's consent.

The first step is to create a new Project in the **Google API console** and set up credentials to obtain a valid **Client ID, Client Secret, API key**, and enable all the required APIs (Drive, Gmail, Sheets, Calendar, etc.).

You also have to define which [scope](https://developers.google.com/identity/protocols/oauth2/scopes) your application will be able to manage. Follow instructions provided in the [API Console Help](https://support.google.com/googleapi/answer/7037264) for detailed instructions and How-To. To create your project and credentials, visit the page [Google API Console](https://console.developers.google.com/apis/credentials).

![Google API credentials](/credentials1.png)

Client ID, Client Secret, API key and Scope can be created/defined **only once by the developer**. Then, the user has to authorize the application to get a valid **access token** and a **refresh token** to renew the access token when expired.

The easiest way to use Google's APIs would be using Google's JavaScript OAuth 2.0 for Client-side library provided from Google itself in a webpage hosted on the ESP device. Unfortunately, only top-level domains can be whitelisted as "Authorized JavaScript origins," so this way is impracticable for our purposes.

This library will authorize your device as a **Web Service** using the OAuth 2.0 endpoint for [Web Server Applications](https://developers.google.com/identity/protocols/oauth2/web-server), and a little web server hosted on ESP is necessary. For your (or your users') convenience, the provided **http://(your_device_address)/token** page can handle all necessary steps in the background and finally save the JSON configuration file in the ESP memory.

Note that users will be warned about an "unverified App" unless you submit a request to Google for verification if you require it. You can simply skip it for testing purposes or if you don't need to redistribute: just click on _advanced_ and then go to _---your app name---(unsafe)_.

**Important**

As you can read in the Google documentation, for this authorization scenario, it is necessary to have an authorized redirect URI as endpoints to which the OAuth 2.0 server can send the generated tokens. If you own a top-level host, I've written a small PHP script, and you can upload it to your server and use it as a redirect URI.

Another good option for a redirect URI is using a micro-service like [pipedream.com](https://pipedream.com), for example. The free plan will be largely sufficient for most cases (remember, you will need it only once for each user). This is my pipedream [working workflow](https://pipedream.com/@cotestatnt/smart-air-p_mkCk3JW), feel free to copy and use it for your convenience. The redirect URI to be used will be the one generated when you deploy your workflow (something like xxxxxxxxxx.m.pipedream.net).

The redirect URI has to be inserted in your OAuth 2.0 Client ID credential definition (edit it after creating). Remember also to add https://apis.google.com as an authorized JavaScript origin.

![OAuth 2.0 Client ID](/credentials2.png)

### Features

- Authorize application for the allowed scopes
- Save and refresh access token (local filesystem)

GoogleSheetAPI Class

- Create spreadsheet on Google Drive
- Create sheets in a provided spreadsheet
- Append rows to spreadsheet (formula supported)

GoogleDriveAPI Class

- Create folders on Google Drive
- Upload files on Google Drive
- Update existing files on Google Drive
- Search for a specific file or folder

GoogleGmailAPI Class

- Get list of unread messages
- Read metadata of message
- Read message body (snippet or full text)
- Send message to specific address

### To do

- Add support for Calendar API
- Download files (Google Drive API)
- Set metadata for message (Gmail API)

### Supported boards

The library works with the ESP8266 and ESP32 chipset.

- 1.0.4 Webserver included in the source (no need to load Data folder in the filesystem).
- 1.0.3 Added support for Google Mail API
- 1.0.2 Added support for Google Sheet API
- 1.0.1 Added support for Google Drive API
- 1.0.0 Initial version
