/*
  DIY VeriLight / ReadyLight for FinishLynx
  by Christoph, 2024, Altered by Gillian Goud, 2025

  Saves data in JSON file on ESP32
  Uses SPIFFS

  The Wifi-Manager functions are based on Bills codes at DroneBot Workshop
  https://dronebotworkshop.com/wifimanager/

  Functions based upon sketch by Brian Lough
  https://github.com/witnessmenow/ESP32-WiFi-Manager-Examples
*/

#include <WiFi.h>         // WiFi Library
#include <FS.h>           // File System Library
#include <SPIFFS.h>       // SPI Flash Syetem Library
#include <WiFiManager.h>  // WiFiManager Library
#include <ArduinoJson.h>  // Arduino JSON library
#include <Wire.h>         // 2 Wire communication
#include <Adafruit_GFX.h> // Library for Screen
#include <Adafruit_SSD1306.h> // Library for OLED Screen

#define ESP_DRD_USE_SPIFFS true
#define TRIGGER_PIN 13                          // Define Wifi-Reset PIN
#define JSON_CONFIG_FILE "/config.json"    // JSON configuration file
#define SCREEN_WIDTH 128                        // OLED display width, in pixels
#define SCREEN_HEIGHT 64                        // OLED display height, in pixels

#define PIN_GREEN 17
#define PIN_YELLOW 18
#define PIN_RED 18

// Flag for saving data
bool shouldSaveConfig = false;

// Variables to hold data from custom textboxes
char testString[50] = "test value";
int LynxPort = 10000;

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);


WiFiManager wm;         // Define WiFiManager Object

WiFiServer server(LynxPort);

int sizeMsg = 0;
int timeout = 120;    // seconds for AccessPoint to run for after Button Press

// Change to true when testing to force configuration every time we run
bool forceConfig = false;

//timing related variables
unsigned long previousMillis = 0;
const long interval = 1000;  // interval at which to blink (milliseconds)

int colbyte = 0;
int colbytestor = 0;
int blinkbyte = 0;
bool blinkState = false;

// Function to light any color depending on 7th bit
void lighton(int a) {
    switch(a) {
      // case 0: break; // Originally Black
      case 1:
        digitalWrite(PIN_GREEN, LOW);
        digitalWrite(PIN_YELLOW, LOW);
        digitalWrite(PIN_RED, HIGH);
        break; // Originally Red
      // case 2: ;break; // Originally Gold
      // case 3: ;break; // Originally GreenYellow
      case 4:
      digitalWrite(PIN_RED, LOW);
      digitalWrite(PIN_YELLOW, LOW);
      digitalWrite(PIN_GREEN, HIGH);
      break; // Originally Green
      // case 5: break; // Originally SkyBlue
      // case 6: break; // Originally Blue
      // case 7: break; // Originally Purple
      // case 8: break; // Originally Pink
      // case 9: break; // Originally White
      default:
        digitalWrite(PIN_RED, LOW);
        digitalWrite(PIN_YELLOW, LOW);
        digitalWrite(PIN_GREEN, LOW);
        break; // Originally Black
    }
}

void idleLight() {
  // Blink code
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    digitalWrite(PIN_YELLOW, !digitalRead(PIN_YELLOW));
  }
}

void saveConfigFile() { // Save Config in JSON format
  Serial.println(F("Saving configuration..."));

  JsonDocument json;
  json["testString"] = testString;
  json["LynxPort"] = LynxPort;

  File configFile = SPIFFS.open(JSON_CONFIG_FILE, "w");
  if (!configFile) {
    Serial.println("failed to open config file for writing");
  }

  serializeJsonPretty(json, Serial);
  if (serializeJson(json, configFile) == 0) {
    Serial.println(F("Failed to write to file"));
  }

  configFile.close(); // Close file
}

bool loadConfigFile() { // Load existing configuration file
  // Uncomment if we need to format filesystem
  // SPIFFS.format();
  Serial.println("Mounting File System...");

  // May need to make it begin(true) first time you are using SPIFFS
  if (SPIFFS.begin(false) || SPIFFS.begin(true)) {
    Serial.println("mounted file system");
    if (SPIFFS.exists(JSON_CONFIG_FILE)) {
      Serial.println("reading config file");
      File configFile = SPIFFS.open(JSON_CONFIG_FILE, "r");
      if (configFile) {
        Serial.println("Opened configuration file");
        JsonDocument json;
        DeserializationError error = deserializeJson(json, configFile);
        serializeJsonPretty(json, Serial);
        if (!error) {
          Serial.println("Parsing JSON");
          strcpy(testString, json["testString"]);
          LynxPort = json["LynxPort"].as<int>();
          return true;
        } else {
          Serial.println("Failed to load json config");
        }
      }
    }
  } else {
    Serial.println("Failed to mount FS");
  }

  return false;
}

void saveConfigCallback() { // Callback notifying us of the need to save configuration
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void configModeCallback(WiFiManager *myWiFiManager) { // Called when config mode launched
  Serial.println("Entered Configuration Mode");

  Serial.print("Config SSID: ");
  Serial.println(myWiFiManager->getConfigPortalSSID());

  Serial.print("Config IP Address: ");
  Serial.println(WiFi.softAPIP());

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("--------------------");
  display.println("Entered Config. Mode");
  display.println("Config SSID: ");
  display.println(myWiFiManager->getConfigPortalSSID());
  display.println("Config IP: ");
  display.println(WiFi.softAPIP());
  display.display();
}

void setBreakAfterConfig(boolean shouldBreak);

void setup() {
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  pinMode(PIN_GREEN, OUTPUT);
  pinMode(PIN_YELLOW, OUTPUT);
  pinMode(PIN_RED, OUTPUT);

  wm.setConfigPortalTimeout(timeout);

  Serial.begin(115200);
  delay(10);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0xBC)) { // Address 0x3D for 128x64 or 0xBC
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  delay(2000);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("    VeriLight DIY   ");
  display.println("--------------------");
  display.println("Check Configuration");
  display.println("Trigger for next");
  display.println("3 sec");
  display.display();

  Serial.println("Check Configuration Trigger for next 3 sec");
  unsigned long runtime = millis();
  unsigned long starttime = runtime;
  while ((runtime - starttime) < 3000) {
    if (digitalRead(TRIGGER_PIN) == LOW) {
      Serial.println("Configuration manually triggered.");

      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0, 0);
      display.println("    VeriLight DIY   ");
      display.println("--------------------");
      display.println("Config. triggered.");
      display.println("Entering AP-Mode.");
      display.display();
      forceConfig = true;
    }
    Serial.print(".");
    display.print(".");
    display.display();
    delay(500);
    runtime = millis();
  }

  bool spiffsSetup = loadConfigFile();
  if (!spiffsSetup) {
    Serial.println(F("Forcing config mode as there is no saved config"));
    forceConfig = true;
  }

  // Explicitly set WiFi mode
  WiFi.mode(WIFI_STA);

  // Reset settings (only for development)
  //wm.resetSettings();

  // Set config save notify callback
  wm.setSaveConfigCallback(saveConfigCallback);

  // Set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wm.setAPCallback(configModeCallback);

  //exit after config instead of connecting
  //wm.setBreakAfterConfig(true);

  // Custom elements

  // Text box (String) - 50 characters maximum
  WiFiManagerParameter custom_text_box("key_text", "Enter your string here", testString, 50);

  // Need to convert numerical input to string to display the default value.
  char convertedValue[6];
  sprintf(convertedValue, "%d", LynxPort);

  // Text box (Number) - 5 characters maximum
  WiFiManagerParameter custom_text_box_num("key_num", "Communication Port", convertedValue, 5);

  // Add all defined parameters
  wm.addParameter(&custom_text_box);
  wm.addParameter(&custom_text_box_num);

  Serial.println(F("Reached"));

  if (false) { // Run if we need a configuration -- forceConfig
    if (!wm.startConfigPortal("ReadyLight_AP", "12345678")) {
      Serial.println("failed to connect and hit timeout");
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("    VeriLight DIY   ");
      display.println("--------------------");
      display.println("failed to connect");
      display.println("and hit timeout");
      display.println("----- RESTART -----");
      display.display();
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.restart();
      delay(5000);
    }
  } else {
    if (!wm.autoConnect("ReadyLight_AP", "12345678")) {
      Serial.println("failed to connect and hit timeout");
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("    VeriLight DIY   ");
      display.println("--------------------");
      display.println("failed to connect");
      display.println("and hit timeout");
      display.println("----- RESTART -----");
      display.display();
      delay(3000);
      // if we still have not connected restart and try all over again
      ESP.restart();
      delay(5000);
    }
  }

  // If we get here, assume we are connected to the WiFi
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("    VeriLight DIY   ");
  display.println("--------------------");
  display.print("IP: ");
  display.println(WiFi.localIP());
  display.print("Port: ");
  display.println(LynxPort);
  display.display();

  server.begin(LynxPort);

  // Lets deal with the user config values

  // Copy the string value
  strncpy(testString, custom_text_box.getValue(), sizeof(testString));
  Serial.print("testString: ");
  Serial.println(testString);

  //Convert the number value
  LynxPort = atoi(custom_text_box_num.getValue());
  Serial.print("LynxPort: ");
  Serial.println(LynxPort);


  // Save the custom parameters to FS
  if (shouldSaveConfig) {
    saveConfigFile();
    ESP.restart();
  }
}




void loop() {
  WiFiClient client = server.available();    // Listen for incoming clients
  idleLight();

  if (client) {                             // If a new client connects,
    if (client.connected()) {
      Serial.println("Client connected.");  // print a message out in the serial port
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("    VeriLight DIY   ");
      display.println("--------------------");
      display.print("IP: ");
      display.println(WiFi.localIP());
      display.print("Port: ");
      display.println(LynxPort);
      display.println("--------------------");
      display.println("  CLIENT CONNECTED  ");
      display.display();
    }
    while (true) {                          // loop while the client's connected
      if (client.available()) {
        delay(10);                            // delay to allow all bytes to be transferred
        sizeMsg = client.available();         // number of bytes transferred
        char charArray[sizeMsg];            // create array with the size of bytes (-1 because array starts with 0) --> That makes no sense, as the size does not get counted

        Serial.print("I received: ");
        Serial.print(sizeMsg);
        Serial.println(" bytes.");

        for (int i=0; i < sizeMsg; i++) { // for-loop to fill the array wiht the transferred bytes
            charArray[i] = client.read();     // fill the array

            Serial.print("Byte ");
            Serial.print(i+1);
            Serial.print(": ");
            Serial.println(charArray[i], HEX);  //print out the content of the array at i
        }

        if (sizeMsg > 7) {
          colbyte = charArray[6];                   // convert 7th byte into int
          colbytestor = charArray[6];               // create a colbytestorage for blinking
          Serial.print("The 7th bit is: ");
          Serial.print(colbyte);
          if (sizeMsg > 12) {
            blinkbyte = charArray[12];
            Serial.print("The blinkbit is: ");
            Serial.print(blinkbyte);
          }
        } else {
          Serial.println("Msg smaller than 7 byte - LED off."); //Wenn Initialisiert wird, die Zeit läuft oder pausiert ist (weniger als 7 bytes), keine Freigabe; d.h. ABC000
          // TODO: Turn All lights off
          colbyte = 0;
        }
      }
      unsigned long currentMillis = millis();
      if (blinkbyte == 1) { // check if the blinkbyte is 1
        if (currentMillis - previousMillis >= interval) {
          previousMillis = currentMillis;
          if (colbyte == colbytestor) {
            colbyte = 0;
          } else {
            colbyte = colbytestor;
          }
          lighton(colbyte);
        }
      } else {
          lighton(colbyte);                         // call lighton function, color dependend on 7th byte, blinking dependend on 13th byte (off by default)
      }

      if (!client.connected()) { // if the client disconnects
        Serial.println("Client disconnected.");
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("    VeriLight DIY   ");
        display.println("--------------------");
        display.print("IP: ");
        display.println(WiFi.localIP());
        display.print("Port: ");
        display.println(LynxPort);
        display.println("--------------------");
        display.println(" CLIENT DISCONNECTED");
        display.display();
        client.stop();                            // stop the client funtion
        // TODO: Turn All lights off
        break;                                    // leave the for loop to be ready for new connection
      }
    }
  }
}
