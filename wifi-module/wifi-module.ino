/*!
 * @file wifi-module.ino
 *
 * @section intro_sec Introduction
 *
 * This file contains the implementation of the code that runs on the WiFi module (ESP-01s)
 *
 *  The builtin LED is available on GPIO2 Pin in the ESP-01s schematics.
 *  This LED should provide the visual confirmation of whether the chip
 *  is running on the normal(Standby) mode or WiFi settings configuration mode.
 *  During the configuration mode, the LED should either be blinking or consistently
 *  on. The configurations mode only runs once immediately after the chip boots up
 *  until the Station mode connectivity is established. If the builtin LED is ON
 *  but is not blinking, connect to the chip via its configuration A.P. mode and
 *  update the settings. The second confirmation of the configurations mode being
 *  active is that, AP_SSID appears on the scanned available WiFi networks.
 *  When the builtin LED goes off, the connection has been established and the WiFi
 *  module is running on the stand-by(normal) waiting for actual the HTTP requests.
 *
 * @section author Author
 *
 * Written by dmigwi (Migwi Ndung'u)  @2024
 * LinkedIn: https://www.linkedin.com/in/migwi-ndungu/
 *
 *  @section license License
 *
 * BSD license, all text here must be included in any redistribution.
 */

#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>

// The text of builtin files are in this header file
#include "builtinfiles.h"

// uncomment next line to get debug messages output to Serial link
// #define DEBUG

namespace Settings
{
    // SERIAL_BAUD_RATE defines the speed of communication on the serial interface.
    const long int SERIAL_BAUD_RATE {115200};

    // EEPROM_SIZE defines the EEPROM memory size to initialize.
    const int EEPROM_SIZE {320};

    // When setting up the AP mode is used to set the Station WiFi settings the
    // following configuration is used:
    const char* AP_SSID {"RFID-based-Auth"};
    const char* AP_Password  {"Adm1n$tr8oR"};
    // WiFi_Hostname defines part of the WiFi hostname. The full name with
    // "RFID_AUTH_24xMac" where "24xMac" represents the last 24 bits of the mac address.
    const char* WiFi_Hostname {"RFID_AUTH"};

    // multicast DNS is used to attach '.local' suffix to create the complete domain name
    // (http://rfid_auth.local). This url is mapped to the server running in the AP network.
    const char* AP_Server_Domain {"rfid_auth"};

    // AP_SERVER_PORT defines the default at which the webserver will serve traffic
    // during the Access Point mode of the chip.
    const int AP_Server_Port {80};

    // SERVER_API_URL defines the URL to which the HTTP client will make API calls to.
    const char* SERVER_API_URL {"http://dmigwi.atwebpages.com/auth/time.php"};

    const byte MAX_SSID_LEN {32}; // A max of 32 characters allowed.
    const byte MAX_PASS_LEN {64}; // A max of 64 characters allowed.

    // Delay Timeout is set to 1 minute (6000ms) after which, the network
    // connection attempts are aborted.
    const int CONNECTION_TIMEOUT {60000};

    // REFRESH_DELAY defines the interval at which components are refreshed.
    const int REFRESH_DELAY {1000};

    // STORAGE_ADDRESS defines the location where wifi setting will be stored in the
    // EEPROM storage.
    const int STORAGE_ADDRESS {0};

    // ESP-01 and ESP-01S are both programmed using the Generic ESP8266
    // settings but have different pins for the builtin LED. Select Builtin
    // Led:1 for the ESP-01 and Builtin Led:2 for the ESP-01S
    const byte LED {2};

     // AuthInfo defines parameters needed to connect to a WiFi channel.
    typedef struct
    {
        char WiFiName[MAX_SSID_LEN]; // WiFi SSID name
        char password[MAX_PASS_LEN]; // WiFi Auth Password
    } AuthInfo;
};


// blinkBuiltinLED triggers the LED to toggle on and off.
void blinkBuiltinLED(int timeout)
{
    digitalWrite(Settings::LED, (digitalRead(Settings::LED)==HIGH) ? LOW : HIGH);
    Serial.print(".");
    delay(timeout);
}

class WebServer
{
    public:
        // configures the access point to be used in serving static web pages.
        // The preset copy of SSID auth keys info is passed.
        WebServer(Settings::AuthInfo authKeys)
        : m_authKeys{authKeys}
        {
            WiFi.mode(WIFI_AP);
            bool isSet = WiFi.softAP(Settings::AP_SSID, Settings::AP_Password);
            printConnectionStatus(isSet);

            isSet  = MDNS.begin(Settings::AP_Server_Domain);
            #ifdef DEBUG
            if (isSet)
                Serial.println(F("MDNS responder is running on: http://rfid_auth.local"));
            #endif
        }

        // handleRoot returns the webpage to be viewed when a GET request
        // is made to (http://rfid_auth.local).
        void handleRoot(void)
        {
            char buffer[1500];  // Preallocate a large chunk to avoid memory fragmentation
            sprintf(buffer, rootContent, m_authKeys.WiFiName, Settings::MAX_SSID_LEN,
                Settings::MAX_SSID_LEN, m_authKeys.password, Settings::MAX_PASS_LEN,
                Settings::MAX_PASS_LEN);
            m_server.send(200, "text/html", buffer);
        }

        // handleShutdown enables the user to safely exit the configuration setup mode by shutting down
        // the server and resetting the WiFi mode to Station for the normal WiFi module running.
        void handleShutdown(void)
        {
            m_server.send(404, "text/html", FPSTR(exitConfigModeContent));

            #ifdef DEBUG
            Serial.println(F("Shutting down the server!"));
            #endif
            m_server.stop();

            // Wait for the server to completely shutdown.
            delay(Settings::REFRESH_DELAY);

            #ifdef DEBUG
            Serial.println(F("Shutting the EEPROM Interface!"));
            #endif
            // Shutdown the EEPROM Interface
            EEPROM.end();

            #ifdef DEBUG
            Serial.println(F("Restarting the ESP-01 chip CPU"));
            #endif
            ESP.restart();
        }

        // handleUpdateSettings extracts the sent WiFi parameters and attempts to write them into the
        // preset EEPROM memory address. If on trimming the white characters either the network name
        // or password are found to be empty, the update will fail.
        void handleUpdateSettings(void)
        {
            // updateStatus string is included in the update status webpage to indicate
            // update failure if it happened.
            char updateStatus[] = "NOT";

            if (m_server.hasArg("ssid") && m_server.hasArg("pwd")) {
                String ssid = m_server.arg("ssid");
                String pwd = m_server.arg("pwd");
                ssid.trim(); // Remove trailing and preceding whitespace characters.
                pwd.trim(); // Remove trailing and preceding whitespace characters.

                #ifdef DEBUG
                Serial.printf("Detected SSID : '%s' \n Detected Password : '%s' \n", ssid.c_str(), pwd.c_str());
                #endif

                if (ssid.length() > 0 && pwd.length() > 0)
                {
                    ssid.toCharArray(m_authKeys.WiFiName, pwd.length()+1); // set the ssid string to the struct.
                    pwd.toCharArray(m_authKeys.password, ssid.length()+1); // set the pwd string to the struct.

                    // Update the EEPROM with the latest settings.
                    EEPROM.put(Settings::STORAGE_ADDRESS, m_authKeys.password);
                    // Commit the EEPROM Changes.
                    EEPROM.commit();

                    // Remove the failure emphasis text indicating success of the update operation.
                    updateStatus[0] = '\0'; // Clearing the char array.
                }
            }

            char buffer[1000];  // Preallocate a large chunk to avoid memory fragmentation
            sprintf(buffer, updateSuccessfulContent, updateStatus, m_authKeys.WiFiName,
                m_authKeys.password);
            m_server.send(200, "text/html", buffer);
        }

        // printConnectionStatus prints the status of the AP connectivity.
        // Works only during the debugging mode.
        void printConnectionStatus(bool isSet)
        {
           #ifdef DEBUG
            Serial.printf("Initializing soft-AP %s \n AP SSID: %s \n AP Password: '%s' \n Server IP Address %s ",
                isSet ? "Ready" : "Failed!", Settings::AP_SSID, Settings::AP_Password, WiFi.softAPIP().toString().c_str()
            );
            #endif
        }

        // configureServer sets routes handlers to all the routes supported.
        void configureServer()
        {
            m_server.on("/", [&](){ handleRoot(); });
            m_server.on("/quit", [&](){ handleShutdown(); });
            m_server.on("/update", [&](){ handleUpdateSettings(); });

            // handle cases when file is not found
            m_server.onNotFound([&]() {
                m_server.send(404, "text/html", FPSTR(notFoundContent));
            });

            // enable CORS header in webserver results
            m_server.enableCORS(true);

            // enable ETAG header in webserver results from serveStatic handler
            m_server.enableETag(true);

            m_server.begin();

            #ifdef DEBUG
            Serial.println("HTTP server started");
            #endif
        }

        // Listens to incoming http requests and handles them.
        void listen()
        {
            m_server.handleClient();
            MDNS.update();
        }
    private:
        // m_server indicates the created server interface
        ESP8266WebServer m_server{Settings::AP_Server_Port};

        Settings::AuthInfo m_authKeys{};
};

class WiFiConfig
{
    public:
        // WifiConfig Constructor.
        WiFiConfig() = default;

        // setUpConfig sets up the device and initializes the EEPROM
        void setUpConfig()
        {
            #ifdef DEBUG
            Serial.println(F("\n Booting the WIFI Module"));
            #endif

            byte macAddr[6];
            WiFi.macAddress(macAddr); // Extracts the mac address.

            // Sets the WiFi DEVICE_ID details which should not exceed 32
            // characters otherwise it will not be effected.
            char deviceID[Settings::MAX_SSID_LEN];
            sprintf(deviceID, "%s-%02x%02x%02x",  Settings::WiFi_Hostname,
                macAddr[3], macAddr[4],  macAddr[5]
            );
            WiFi.hostname(deviceID);

            // Init the EEPROM.
            EEPROM.begin(Settings::EEPROM_SIZE);

            // Extract Memory Contents.
            EEPROM.get(Settings::STORAGE_ADDRESS, m_settings);
        }

        // establishConnection turns the Station Mode on and allows the chip to
        // attempt connecting to an external access point. If the connection
        // establishment times out, the chip's Access Point mode is turned on
        // allowing the invalid SSID and password information.
        // When in station mode and a connection has been established the function
        // exist allowing the chip to handle other functions like serial communication.
        // When in access point mode, the chip enters an infinite server loop that
        // can only be exited by restartng the chip.
        void establishConnection()
        {
            printConnectionLog();

            // The device by default it runs on Station mode. AP mode only used to update
            // station mode WIFI Settings.
            WiFi.mode(WIFI_STA);
            WiFi.begin(m_settings.WiFiName, m_settings.password);

            // Automatic Light Sleep at DTIM listen interval of 3 allows the WiFi module
            // to easily establish and maintain connections.
            // https://github.com/esp8266/Arduino/blob/685f2c97ff4b3c76b437a43be86d1dfdf6cb33e3/libraries/ESP8266WiFi/src/ESP8266WiFiGeneric.cpp#L281-L305
            WiFi.setSleepMode(WIFI_LIGHT_SLEEP, 3);

            // isWiFiConnection waits for the wifi connection status to change to "connected"
            // or the timeout to expire; whichever comes first. It then returns a boolean value
            // indicating if the connection was successful or not.
            auto isWiFiConnection = [this]()-> bool {
                // Wait for connection success status only till connection timeout.
                size_t timeout = millis() + Settings::CONNECTION_TIMEOUT;
                while (WiFi.status() != WL_CONNECTED && millis() <= timeout)
                    blinkBuiltinLED(Settings::REFRESH_DELAY/2);

                digitalWrite(Settings::LED, HIGH);
                Serial.println(".");

                this->printConnectionStatus();

                return WiFi.isConnected();
            };

            if (!isWiFiConnection()) {
                // Wifi connection via Station mode failed. Set the WiFi mode to AP and attempt
                // to configure the station mode wifi settings.
                WebServer server {m_settings};
                server.configureServer();
                digitalWrite(Settings::LED, LOW);

                // The only way to exit this infinite loop is to restart the WiFi module CPU after
                // setting the correct Station mode WiFi settings.
                while(1)
                    server.listen();
            }

            // Shutdown the EEPROM interface
            EEPROM.end();

            #ifdef DEBUG
            Serial.println("Station WiFi Mode Active");
            #endif

            // blink 5 times to indicate that WiFi connectivity is working as expected.
            for (int i{0}; i<5; i++)
                blinkBuiltinLED(Settings::REFRESH_DELAY);
        }

        // printChipVersion prints the esp chip version details identified.
        // Works only during the debugging mode.
        void printChipVersion()
        {
            #ifdef DEBUG
            Serial.printf(" Core Version: %s \n SDK Version: %s \n Chip ID: %d \n Reset Reason: %s \n",
                ESP.getCoreVersion().c_str(), ESP.getSdkVersion(), ESP.getChipId(),
                ESP.getResetReason().c_str()
            );
            #endif
        }

        // printConnectionLog prints the SSID, password and hostname details of the
        // WiFi network currently being considered. Works only during the debugging mode.
        void printConnectionLog()
        {
            #ifdef DEBUG
            Serial.println(F("\n Establishing Station Mode Connection!"));
            Serial.println(F(" Attempting connection to the external AP!"));

            Serial.printf(" SSID: '%s' \n Password: '%s' \n Device ID: %s \n",
                m_settings.WiFiName, m_settings.password, WiFi.hostname().c_str()
            );

            #endif
        }

        // PrintConnectionStatus prints a message if the WIFI connection was successful of not.
        // Works only during the debugging mode.
        void printConnectionStatus()
        {
            #ifdef DEBUG
            if (WiFi.isConnected())
            {
                Serial.println(F("\n  Connection to AP was Successful!"));
                Serial.printf(" IP Address: %s \n", WiFi.localIP().toString().c_str());
            }
            else
                Serial.println(F("Connecting to Wifi AP Failed!"));
            #endif
        }

        // handleEvents deals with the core serial communication between the Server
        // PCD Serial interface
        void handleEvents()
        {
            // Switch off builtin status light.
            digitalWrite(Settings::LED, HIGH);

            // The WiFi connection is active and serial data has been received
            if (WiFi.status() == WL_CONNECTED && Serial.available()) {
                // Switch on builtin status light to indicate API connection activity.
                digitalWrite(Settings::LED, LOW);

                // Read the contents of the serial buffer.
                size_t bufferSize = Serial.available();
                char buffer[bufferSize];
                byte counter = 0;
                while (Serial.available()) {
                    buffer[counter] = Serial.read();
                    counter++;
                }

                WiFiClient client{};
                HTTPClient http{};

                #ifdef DEBUG
                Serial.println("[HTTP] Setting the client config");
                #endif

                // configure traged server and url
                http.begin(client, Settings::SERVER_API_URL);  // HTTP
                http.addHeader("Content-Type", "application/json");

                #ifdef DEBUG
                Serial.println("[HTTP] Make a POST Request");
                #endif

                // start connection and send HTTP header and body
                int httpCode = http.POST(String(buffer));

                // httpCode will be negative on error
                if (httpCode > 0) {
                    // HTTP header has been send and Server response header has been handled
                    #ifdef DEBUG
                    Serial.printf("[HTTP] POST... code: %d\n", httpCode);
                    #endif

                    // file found at server
                    if (httpCode == HTTP_CODE_OK) {
                        const String& payload = http.getString();
                        Serial.println("received payload:\n<<");
                        Serial.println(payload);
                        Serial.println(">>");
                    }
                } else {
                    #ifdef DEBUG
                    Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
                    #endif
                }

                http.end();
            }
        }

    private:
        Settings::AuthInfo m_settings{};
};

WiFiConfig config{};

// Sets up the WiFi config
void setup()
{
    // Initialize the serial interface.
    Serial.begin(Settings::SERIAL_BAUD_RATE);

    // Set the GPIO2 Pin as output and set it LOW.
    pinMode(Settings::LED, OUTPUT);
    digitalWrite(Settings::LED, LOW);

    #ifdef DEBUG
    while(!Serial){
        ; // wait for serial port to connect.
    }
    #endif

    config.setUpConfig(); // Set up Device ID and EEPROM
    config.printChipVersion();  // Log the chip version in debug mode

    for (int i {0}; i<5; ++i)  // Delay to allow the serial interface to be ready..
        blinkBuiltinLED(Settings::REFRESH_DELAY);

    digitalWrite(Settings::LED, LOW); // Turn off the LED after blinking

    config.establishConnection();
}

void loop()
{
    config.handleEvents();
}
