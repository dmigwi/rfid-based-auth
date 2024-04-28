/**
 * @file builtinfiles.h
 * @brief This file is part of the WebServer example for the wifi-module.
 *  
 * This file contains long, multiline text variables for  all builtin resources.
 */

#ifndef BUILTIN_FILES_
#define BUILTIN_FILES_

// rootContent defines content for the route http://rfid.auth.local/
static const char rootContent[] PROGMEM = R"==(
    <!DOCTYPE html>
    <html lang="en-GB">
    <head><title>WiFi Configuration</title></head>
    <body>
        <p style="font-weight: bold;">RFID-based Auth Configuration.</p>
        <span>Use this form to configure esp8266 WiFi settings for use in the Station(STA) Mode.</span><br>
        <i>Leading and trailing whitespace characters are trimmed!</i>
        <form method="POST" action="update">
            <p>SSID :<input type="text" placeholder="Network Name" name="ssid" value="%s" maxlength="%d" required/>
                <i>Max characters allowed (%d).</i>
            </p>
            <p>PWD :<input type="text" placeholder="Password" name="pwd" value="%s" maxlength="%d" required/>
                <i>Max characters allowed (%d).</i> Password for WEP/WPA/WPA2.
            </p>
            <input type="submit" value="Configure"/>
        </form>
        <p><a href="/quit">Quit and Restart WiFi module!</a></p>
    </body>
    </html>
    )==";

// notFoundContent defines the 404 error content.
static const char notFoundContent[] PROGMEM = R"==(
    <!DOCTYPE html>
    <html lang="en-GB">
    <head>
    <title>Not found</title>
    </head>
    <body>
    <h1>404 - Not Found!</h1>
    <p><a href="/">Start Configuration Again!</a></p>
    or
    <p><a href="/quit">Quit and Restart WiFi module!</a></p>
    </body>
    </html>
    )==";

// updateSuccessfulContent defines content for the route http://rfid.auth.local/update
static const char updateSuccessfulContent[] PROGMEM = R"==(
    <!DOCTYPE html>
    <html lang="en-GB">
    <head>
    <title>Update Successful</title>
    </head>
    <body>
    <h4>The WiFi Configuration Update Was <span style="color:red;">%s</span> Successful!!! :)</h4>
    <i>Current WiFi Settings for Use in the Station (STA) Mode</i>
    <p><i>WiFi Access Point Name (SSID):</i> "<strong>%s</strong>"</p>
    <p><i>WiFi Password (WEP/WPA/WPA2):</i> "<strong>%s</strong>"</p>
    <p><a href="/">Reset the WiFi configuration!</a></p>
    or
    <p><a href="/quit">Quit and Restart WiFi module!</a></p>
    </body>
    )==";

// exitConfigModeContent defines content for the route http://rfid.auth.local/quit
static const char exitConfigModeContent[] PROGMEM = R"==(
    <!DOCTYPE html>
    <html lang="en-GB">
    <head>
    <title>Bye Bye</title>
    </head>
    <body>
    <h1>Bye! :)</h1>
    <em>WiFi Module is restarting, therefore exiting this settings configuration mode!</em>
    </body>
    </html>
    )==";
  
#endif