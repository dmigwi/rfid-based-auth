#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

// The text of builtin files are in this header file
#include "builtinfiles.h"

// BUILTIN_LED defines the builtin led on GPIO2 in the ESP-01s schematics.
// This LED should provide the visual confirmation of whether the chip is
// running on the normal mode or WiFi settings configuration mode.
// During normal mode, the LED should blink atleast every 1secs,
// if it stays on for longer than that most likely the WiFi configuration mode
// is active.
// The second confirmation of the WiFi settings mode being active should be the
// AP_SSID appearing on the available networks on scanning the visible wireless networks.
#define BUILTIN_LED 2

// uncomment next line to get debug messages output to Serial link
// #define DEBUG

// When setting up the AP mode is used to set the Station WiFi settings the
// following configuration is used:
const String AP_SSID = "RFID-based-Auth";
const String AP_Password = "Adm1n$tr8oR";

// multicast DNS is used to attach '.local' suffix to create the complete domain name
// (http://rfid.auth.local). This url is mapped to the server running in the AP network.
const String AP_Server_Domain = "rfid.auth";

const int SERIAL_BAUD_RATE = 115200;
const int MAX_SSID_LEN = 32; // Max of 32 characters allowed.
const int MAX_PASSWORD_LEN = 64; // Max of 64 characters allowed.

// Delay Timeout is set to 1 minute after which, the network connection
// attempts are aborted.
const int CONNECTION_TIMEOUT = 60000;

// STORAGE_ADDRESS defines the location where wifi setting will be stored in the
// EEPROM storage.
const float STORAGE_ADDRESS = 0;

// Struct Object to hold the station mode wifi settings.
struct WifiConfigSettings{
  char SSID[MAX_SSID_LEN];
  char Password[MAX_PASSWORD_LEN];
};

WifiConfigSettings settings = WifiConfigSettings();

// Server indicates the created server interface
ESP8266WebServer server(80);

void setup() {
  // Set the GPIO2 as output and
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, HIGH);

  Serial.begin(SERIAL_BAUD_RATE);
  // Delay allows the serial interface to be ready.
  for (int i=0;i<5;i++) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println(F("Booting the WIFI Module"));

  // Read the preset Station WIFI Settings from EEPROM.
  EEPROM.get(STORAGE_ADDRESS, settings);
  String ssid = String(settings.SSID);
  String password = String(settings.Password);

  #ifdef DEBUG
  Serial.println(F("Attempting a connection to AP"));
  Serial.print(F("SSID :'"));
  Serial.print(ssid);
  Serial.println(F("'"));
  Serial.print(F("Password :'"));
  Serial.print(password);
  Serial.println(F("'"));
  #endif

  // The device by default it runs on Station mode. AP mode only used to update
  // station mode WIFI Settings.
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);


  if (!isWiFiConnection()) {
    // Wifi connection via Station mode failed. Set the WiFi mode to AP and attempt
    // to configure the station mode wifi settings.
    setUpConfigAP();

    // The only way to exit this infinite loop is to restart the WiFi module CPU after
    // setting the correct Station mode WiFi settings.
    while(1) {
      server.handleClient();
      MDNS.update();
    }
  }

  #ifdef DEBUG
  Serial.println("Station WiFi Mode Active");
  #endif

  // blink 5 times to indicate that WiFi connectivity is working as expected.
  for (int i=0;i<5;i++) {
    digitalWrite(BUILTIN_LED, LOW);
    delay(1000);
    digitalWrite(BUILTIN_LED, HIGH);
  }
}

// the loop function runs over and over again forever
void loop() {
  // digitalWrite(LED_BUILTIN, LOW);  // Turn the LED on (Note that LOW is the voltage level
  // delay(1000);                      // Wait for a second
  // digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
  // delay(1000);                      // Wait for two seconds (to demonstrate the active low LED)
}

/*
  isWiFiConnection waits for the wifi connection status  to change to connected
  of the timeout to expire, whichever comes first. It then returns a boolean value
  indicating if the connection was successful or not.
*/
bool isWiFiConnection() {
  // Wait for connection success status only till connection timeout.
  while (WiFi.status() != WL_CONNECTED && millis() <= CONNECTION_TIMEOUT) {
    delay(500);
    Serial.print(".");
  }

  #ifdef DEBUG
  if (WiFi.isConnected()) {
    Serial.println(F("Connecting to AP was Successful"));
    Serial.print(F("IP Address :'"));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("Connecting to Wifi AP Failed!"));
    Serial.print(F("Error Msg :'"));
    Serial.println(getWiFiStatusMsg());
  }
  #endif
  
  return WiFi.isConnected();
}

/*
  setUpConfigAP configures an Access Point that will be used to configure the WiFi Station
  mode settings.
*/
void setUpConfigAP() {
  WiFi.mode(WIFI_AP);
  bool isSet = WiFi.softAP(AP_SSID, AP_Password);

  #ifdef DEBUG
  Serial.print(F("Initializing soft-AP ... "));
  Serial.println(isSet ? "Ready" : "Failed!");
  
  Serial.print(F("AP SSID :'"));
  Serial.println(AP_SSID);
  Serial.print(F("AP Password :'"));
  Serial.println(AP_Password);
  Serial.print(F("Server IP Address :'"));
  Serial.println(WiFi.softAPIP());
  #endif

  isSet  = MDNS.begin(AP_Server_Domain);
  if (isSet) { 
    #ifdef DEBUG
    Serial.println("MDNS responder started");
    #endif
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/quit", HTTP_GET, handleShutdown);
  server.on("/update", HTTP_POST, handleUpdateSettings);
  // handle cases when file is not found
  server.onNotFound([]() {
    server.send(404, "text/html", FPSTR(notFoundContent));
  });

  // enable CORS header in webserver results
  server.enableCORS(true);

  // enable ETAG header in webserver results from serveStatic handler
  server.enableETag(true);

  server.begin();
  #ifdef DEBUG
  Serial.println("HTTP server started");
  #endif
}

/*
  getWiFiStatusMsg provides a descriptive status message in relation to the wifi
  connection attempts.
*/
String getWiFiStatusMsg() {
  String status = "Enable Debug Status!";
  #ifdef DEBUG
  switch (WiFi.status()) {
    case WL_IDLE_STATUS:
      status = "Wi-Fi is in process of changing between statuses";
      break;
    case WL_NO_SSID_AVAIL:
      status = "Configured SSID cannot be reached";
      break;
    case WL_SCAN_COMPLETED:
      status = "Scan Completed";
      break;
    case WL_CONNECT_FAILED:
      status = "connection failed";
      break;
    case WL_CONNECTION_LOST:
      status = "Connection lost";
      break;
    case WL_WRONG_PASSWORD:
      status = "Wrong password";
      break;
    case WL_DISCONNECTED:
      status = "WiFi disconnected";
      break;
    default:
      status = "Connection Successful";
    }
    #endif
    return status;
}

// Server Pages

/*
  handleRoot returns the webpage to be viewed when a GET request is made to (http://rfid.auth.local).
*/
void handleRoot() {
  char buffer[1500];  // Preallocate a large chunk to avoid memory fragmentation
  sprintf(buffer, rootContent, settings.SSID, MAX_SSID_LEN, MAX_SSID_LEN,
          settings.Password, MAX_PASSWORD_LEN, MAX_PASSWORD_LEN);
  server.send(200, "text/html", buffer);
}

/*
  handleShutdown enables the user to safely exit the configuration setup mode by shutting down
  the server and resetting the WiFi mode to Station for the normal WiFi module running.
*/
void handleShutdown() {
  server.send(404, "text/html", FPSTR(exitConfigModeContent));

  #ifdef DEBUG
  Serial.println(F("Shutting the server!"));
  #endif
  server.stop();

  delay(1500); // Wait for the server to completely shutdown.

  #ifdef DEBUG
  Serial.println(F("Restarting the ESP-01 chip CPU"));
  #endif
  ESP.restart();
}

/**
  handleUpdateSettings extracts the sent WiFi parameters and attempts to write them into the
  preset EEPROM memory address. If on trimming the white characters either the network name
  or password are found to be empty, the update will fail.
*/
void handleUpdateSettings() {
  // updateStatus string is included in the update status webpage to indicate
  // update failure if it happened.
  char updateStatus[] = "NOT";

  if (server.hasArg("ssid") && server.hasArg("pwd")) {
    String ssid = server.arg("ssid");
    String pwd = server.arg("pwd");
    ssid.trim(); // Remove trailing and preceding whitespace characters.
    pwd.trim(); // Remove trailing and preceding whitespace characters.
  
    if (ssid.length() > 0 && pwd.length() > 0) {
      ssid.toCharArray(settings.SSID, ssid.length()); // set the ssid string to the struct.
      pwd.toCharArray(settings.Password, pwd.length()); // set the pwd string to the struct.

      // Update the EEPROM with the latest settings.
      EEPROM.put(STORAGE_ADDRESS, settings);

      // Remove the failure emphasis text indicating success of the update operation.
      updateStatus[0] = '\0'; // Clearing the char array.
    }
  }

  char buffer[1000];  // Preallocate a large chunk to avoid memory fragmentation
  sprintf(buffer, updateSuccessfulContent, updateStatus, settings.SSID, settings.Password);
  server.send(200, "text/html", buffer);
}