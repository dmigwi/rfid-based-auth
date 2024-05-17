/*
    @file This file contains the implementation of the code that runs on the WiFi
    module (ESP-01s)

    LED_BUILTIN defines the builtin LED available on GPIO2 in the ESP-01s
    schematics. This LED should provide the visual confirmation of whether the chip
    is running on the normal(Standby) mode or WiFi settings configuration mode.
    During the configuration mode, the LED should either be blinking or consistently
    on. The configurations mode only runs once immediately after the chip boots up
    until the Station mode connectivity is established. If the builtin light is ON
    but is not blinking, connect to the chip via its configuration A.P. mode and 
    update the settings. The second confirmation of the configurations mode being
    active is that, AP_SSID appears on the scanned available WiFi networks.
    When the builtin light goes off, the connection has been established and the WiFi
    module is running on the stand-by(normal) waiting for actual the HTTP requests.
*/

#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>

// The text of builtin files are in this header file
#include "builtinfiles.h"

// SERIAL_BAUD_RATE defines the speed of communication on the serial interface.
#define SERIAL_BAUD_RATE 115200
// EEPROM_SIZE defines the EEPROM memory size to initialize.
#define EEPROM_SIZE 320

// uncomment next line to get debug messages output to Serial link
#define DEBUG

// When setting up the AP mode is used to set the Station WiFi settings the
// following configuration is used:
const char* AP_SSID = "RFID-based-Auth";
const char* AP_Password = "Adm1n$tr8oR";
// WiFi_Hostname defines part of the WiFi hostname. The full name with
// "RFID_AUTH_24xMac" where "24xMac" represents the last 24 bits of the mac address.
const char* WiFi_Hostname = "RFID_AUTH";

// multicast DNS is used to attach '.local' suffix to create the complete domain name
// (http://rfid_auth.local). This url is mapped to the server running in the AP network.
const char* AP_Server_Domain = "rfid_auth";

// SERVER_API_URL defines the URL to which the HTTP client will make API calls to.
const char* SERVER_API_URL = "http://dmigwi.atwebpages.com/auth/time.php";

const byte MAX_SSID_LEN = 32; // A max of 32 characters allowed.
const byte MAX_PASS_LEN = 64; // A max of 64 characters allowed.

// Delay Timeout is set to 1 minute after which, the network connection.
// attempts are aborted.
const int CONNECTION_TIMEOUT = 60000;

// STORAGE_ADDRESS defines the location where wifi setting will be stored in the
// EEPROM storage.
const int STORAGE_ADDRESS = 0;

// Struct Object to hold the station mode wifi settings.
struct WifiConfigSettings{
  char SSID[MAX_SSID_LEN];
  char Password[MAX_PASS_LEN];
};

WifiConfigSettings settings;

// Server indicates the created server interface
ESP8266WebServer server(80);

void setup() {
  // Initialize the serial interface.
  Serial.begin(SERIAL_BAUD_RATE);

  // Set the GPIO2 Pin as output and set it high.
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.println("");
  #ifdef DEBUG
  Serial.print(F("Core Version: "));
  Serial.println(ESP.getCoreVersion());
  Serial.print(F("SDK Version: "));
  Serial.println(ESP.getSdkVersion());
  Serial.print(F("Chip ID: "));
  Serial.println(ESP.getChipId());
  Serial.print(F("Reset Reason: "));
  Serial.println(ESP.getResetReason());
  #endif

  // Delay allows the serial interface to be ready.
  for (int i=0;i<5;i++) {
    // Toggle ON and OFF LED state.
    digitalWrite(LED_BUILTIN, (digitalRead(LED_BUILTIN)==HIGH) ? LOW : HIGH);
    Serial.print(".");
    delay(1000);
  }

  digitalWrite(LED_BUILTIN, LOW);
  Serial.println(".");

  uint8_t macAddr[6];
  WiFi.macAddress(macAddr);

  // Sets the WiFi DEVICE_ID details which should not exceed 32 characters otherwise
  // It will not be effected.
  char fullDeviceID[MAX_SSID_LEN];
  sprintf(fullDeviceID, "%s-%02x%02x%02x", WiFi_Hostname, macAddr[3], macAddr[4], macAddr[5]);
  WiFi.hostname(fullDeviceID);

  //Init EEPROM
  EEPROM.begin(EEPROM_SIZE);
  Serial.println(F("Booting the WIFI Module"));

  // Read the preset Station WIFI Settings from EEPROM.
  ReadWiFiSettingsInEEPROM();
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
  Serial.print(F("Hostname :'"));
  Serial.print(WiFi.hostname());
  Serial.println(F("'"));
  #endif

  // The device by default it runs on Station mode. AP mode only used to update
  // station mode WIFI Settings.
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  // Automatic Light Sleep at DTIM listen interval of 3 allows the WiFi module
  // to easily establish and maintain connections.
  // https://github.com/esp8266/Arduino/blob/685f2c97ff4b3c76b437a43be86d1dfdf6cb33e3/libraries/ESP8266WiFi/src/ESP8266WiFiGeneric.cpp#L281-L305
  WiFi.setSleepMode(WIFI_LIGHT_SLEEP, 3);

  if (!isWiFiConnection()) {
    // Wifi connection via Station mode failed. Set the WiFi mode to AP and attempt
    // to configure the station mode wifi settings.
    setUpConfigAP();
    digitalWrite(LED_BUILTIN, LOW);

    // The only way to exit this infinite loop is to restart the WiFi module CPU after
    // setting the correct Station mode WiFi settings.
    while(1) {
      server.handleClient();
      MDNS.update();
    }
  }

  // Shutdown the EEPROM interface
  EEPROM.end();

  #ifdef DEBUG
  Serial.println("Station WiFi Mode Active");
  #endif

  // blink 5 times to indicate that WiFi connectivity is working as expected.
  for (int i=0;i<5;i++) {
    // Toggle ON and OFF LED state.
    digitalWrite(LED_BUILTIN, (digitalRead(LED_BUILTIN)==HIGH) ? LOW : HIGH);
    delay(1000);
  }
}

// the loop function runs over and over again forever
void loop() {
  // Switch off builtin status light.
  digitalWrite(LED_BUILTIN, HIGH);
  // The WiFi connection is active and serial data has been received 
  if (WiFi.status() == WL_CONNECTED && Serial.available()) {
    // Switch on builtin status light to indicate API connection activity.
    digitalWrite(LED_BUILTIN, LOW);

    // Read the contents of the serial buffer.
    size_t bufferSize = Serial.available();
    char buffer[bufferSize];
    byte counter = 0;
    
    if (bufferSize > 0) {
      while (Serial.available()) {
        buffer[counter] = Serial.read();
        counter++;
      }
    }

    WiFiClient client;
    HTTPClient http;

    #ifdef DEBUG 
    Serial.println("[HTTP] Setting the client config");
    #endif

    // configure traged server and url
    http.begin(client, SERVER_API_URL);  // HTTP
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
      Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
  }
}

/*
  isWiFiConnection waits for the wifi connection status  to change to connected
  of the timeout to expire, whichever comes first. It then returns a boolean value
  indicating if the connection was successful or not.
*/
bool isWiFiConnection() {
  // Wait for connection success status only till connection timeout.
  size_t timeout = millis() + CONNECTION_TIMEOUT;
  while (WiFi.status() != WL_CONNECTED && millis() <= timeout) {
    // Toggle ON and OFF LED state.
    digitalWrite(LED_BUILTIN, (digitalRead(LED_BUILTIN)==HIGH) ? LOW : HIGH);
    Serial.print(".");
    delay(500);
  }

  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println(".");

  #ifdef DEBUG
  if (WiFi.isConnected()) {
    Serial.println(F("Connecting to AP was Successful"));
    Serial.print(F("IP Address: "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("Connecting to Wifi AP Failed!"));
    Serial.print(F("Error Msg: "));
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
  Serial.print(AP_SSID);
  Serial.println(F("'"));
  Serial.print(F("AP Password :'"));
  Serial.print(AP_Password);
  Serial.println(F("'"));
  Serial.print(F("Server IP Address :'"));
  Serial.print(WiFi.softAPIP());
  Serial.println(F("'"));
  #endif

  isSet  = MDNS.begin(AP_Server_Domain);
  if (isSet) { 
    #ifdef DEBUG
    Serial.println(F("MDNS responder is running on: http://rfid_auth.local"));
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
      status = "Wi-Fi in between statuses";
      break;
    case WL_NO_SSID_AVAIL:
      status = "SSID cannot be reached";
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
      status = "Station mode not active";
      break;
    default:
      status = "Connection Successful";
    }
    #endif
    return status;
}

// Server Pages

/*
  handleRoot returns the webpage to be viewed when a GET request is made to (http://rfid_auth.local).
*/
void handleRoot() {
  char buffer[1500];  // Preallocate a large chunk to avoid memory fragmentation
  sprintf(buffer, rootContent, settings.SSID, MAX_SSID_LEN, MAX_SSID_LEN,
          settings.Password, MAX_PASS_LEN, MAX_PASS_LEN);
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
  Serial.println(F("Shutting the EEPROM Interface!"));
  #endif
  // Shutdown the EEPROM Interface
  EEPROM.end();

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

    #ifdef DEBUG
    Serial.print(F("Detected SSID :'"));
    Serial.print(ssid);
    Serial.println(F("'"));
    Serial.print(F("Detected Password :'"));
    Serial.print(pwd);
    Serial.println(F("'"));
    #endif
  
    if (ssid.length() > 0 && pwd.length() > 0) {
      ssid.toCharArray(settings.SSID, ssid.length()+1); // set the ssid string to the struct.
      pwd.toCharArray(settings.Password, pwd.length()+1); // set the pwd string to the struct.

      // Update the EEPROM with the latest settings.
      EEPROM.put(STORAGE_ADDRESS, settings);
      // Commit the EEPROM Changes.
      EEPROM.commit();

      // Remove the failure emphasis text indicating success of the update operation.
      updateStatus[0] = '\0'; // Clearing the char array.
    }
  }

  char buffer[1000];  // Preallocate a large chunk to avoid memory fragmentation
  sprintf(buffer, updateSuccessfulContent, updateStatus, settings.SSID, settings.Password);
  server.send(200, "text/html", buffer);
}

/*
  ReadWiFiSettingsInEEPROM reads the contents of the STORAGE_ADDRESS an formats the read data.
  If the address read was empty, the default byte stored in that location is 0xff.
  It removes the EEPROM empty char from the data read.
 */
void ReadWiFiSettingsInEEPROM() {
  EEPROM.get(STORAGE_ADDRESS, settings);
  String ssid = replaceEmptyBytes(settings.SSID, MAX_SSID_LEN);
  String pwd = replaceEmptyBytes(settings.Password, MAX_PASS_LEN);

  ssid.toCharArray(settings.SSID, ssid.length()+1); // set the ssid string to the struct.
  pwd.toCharArray(settings.Password, pwd.length()+1); // set the pwd string to the struct.
}
/*
  replaceEmptyBytes it replaces all the 0xff EEPROM empty characters with 0x20 which is a whitespace.
  It then removes the trailing and preceeding whitespaces before reassigning the formatted array
  back to the original array.
*/
String replaceEmptyBytes(char* rawStr, byte charCount){
  char data[charCount];
  for (byte i= 0; i<charCount; i++) {
    byte rawChar = rawStr[i]; 
    if (rawStr[i] == 0xff){ // check for EEPROM empty char value.
      rawChar = 0x20; // insert a whitespace character has hex value of 0x20
    } 
    data[i] = rawChar;
  }

  String val = String(data);
  val.trim(); // Remove preceeding and trailing white spaces.
  return val;
}
