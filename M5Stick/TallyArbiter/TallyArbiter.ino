//
// M5StickCPlus2 Tally Arbiter Listener Client
// Refactored and improved version
//
// Board defines used by Arduino IDE as preprocessor flags
// M5StickC        ARDUINO_m5stack_stickc
// M5StickC-Plus   ARDUINO_m5stack_stickc_plus
// M5StickC-Plus2  ARDUINO_m5stack_stickc_plus2
//

#include <M5StickCPlus2.h>
#include <WebSocketsClient.h>
#include <SocketIOclient.h>
#include <Arduino_JSON.h>
#include <PinButton.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include "Free_Fonts.h"
#include <Wire.h>

// ==============================================================================
// CONFIGURATION CONSTANTS
// ==============================================================================

// Pin Definitions
#define TRIGGER_PIN 0

// Color Definitions
#define GREY 0x0020
#define GREEN 0x0200
#define RED 0xF800

// Display Settings
#define MAX_TEXT_SIZE 5
#define START_BRIGHTNESS 11
#define MAX_BRIGHTNESS 100
#define BRIGHTNESS_INCREMENT 10

// Timing Constants
#define RESET_HOLD_TIME 3000
#define RESTART_HOLD_TIME 5000
#define POWEROFF_HOLD_TIME 10000
#define FLASH_DELAY 500
#define DEBOUNCE_DELAY 50
#define PORTAL_TIMEOUT 120

// Default Server Configuration
#define DEFAULT_TA_HOST "172.16.40.25"
#define DEFAULT_TA_PORT "4455"

// Device Configuration
String listenerDeviceName = "m5StickC-";

// ==============================================================================
// USER CONFIGURATION
// ==============================================================================

bool LAST_MSG = false;  // true = show log messages on tally screen

// Tally Arbiter Server (can be changed via WiFi portal)
char tallyarbiter_host[40] = DEFAULT_TA_HOST;
char tallyarbiter_port[6] = DEFAULT_TA_PORT;

// ==============================================================================
// GLOBAL VARIABLES
// ==============================================================================

// Hardware
PinButton btnM5(37);      // M5 button (Button A)
PinButton btnAction(39);  // Action button (Button B)
Preferences preferences;

// Display State
String prevType = "";
String actualType = "";
String actualColor = "";
int actualPriority = 0;
int currentScreen = 0;  // 0 = Tally Screen, 1 = Settings Screen
int currentBrightness = START_BRIGHTNESS;

// Tally Arbiter
SocketIOclient socket;
JSONVar BusOptions;
JSONVar Devices;
JSONVar DeviceStates;
String DeviceId = "unassigned";
String DeviceName = "Unassigned";
String LastMessage = "";

// Network
WiFiManager wm;
bool networkConnected = false;
bool portalRunning = false;
WiFiManagerParameter *custom_taServer = nullptr;
WiFiManagerParameter *custom_taPort = nullptr;

// ==============================================================================
// PREFERENCES MANAGEMENT
// ==============================================================================

String loadPreference(const char* key, const String& defaultValue = "") {
  preferences.begin("tally-arbiter", true);  // Read-only
  String value = preferences.getString(key, defaultValue);
  preferences.end();
  return value;
}

void savePreference(const char* key, const String& value) {
  preferences.begin("tally-arbiter", false);
  preferences.putString(key, value);
  preferences.end();
}

void loadAllPreferences() {
  logger("Reading preferences", "info-quiet");
  
  String savedDeviceId = loadPreference("deviceid");
  if (savedDeviceId.length() > 0) {
    DeviceId = savedDeviceId;
  }
  
  String savedDeviceName = loadPreference("devicename");
  if (savedDeviceName.length() > 0) {
    DeviceName = savedDeviceName;
  }
  
  String savedHost = loadPreference("taHost");
  if (savedHost.length() > 0) {
    logger("Setting TallyArbiter host as " + savedHost, "info-quiet");
    strncpy(tallyarbiter_host, savedHost.c_str(), sizeof(tallyarbiter_host) - 1);
    tallyarbiter_host[sizeof(tallyarbiter_host) - 1] = '\0';
  }
  
  String savedPort = loadPreference("taPort");
  if (savedPort.length() > 0) {
    logger("Setting TallyArbiter port as " + savedPort, "info-quiet");
    strncpy(tallyarbiter_port, savedPort.c_str(), sizeof(tallyarbiter_port) - 1);
    tallyarbiter_port[sizeof(tallyarbiter_port) - 1] = '\0';
  }
}

// ==============================================================================
// LOGGING
// ==============================================================================

void logger(const String& strLog, const String& strType) {
  Serial.println(strLog);
}

// ==============================================================================
// DISPLAY FUNCTIONS
// ==============================================================================

void initializeDisplay() {
  StickCP2.Display.setRotation(3);
  StickCP2.Display.fillScreen(TFT_BLACK);
  StickCP2.Display.setCursor(0, 0);
  StickCP2.Display.setFreeFont(FSS9);
  StickCP2.Display.setTextColor(WHITE, BLACK);
}

void showBootScreen() {
  StickCP2.Display.fillScreen(TFT_BLACK);
  StickCP2.Display.setCursor(0, 0);
  StickCP2.Display.setFreeFont(FSS9);
  StickCP2.Display.setTextColor(WHITE, BLACK);
  StickCP2.Display.println("booting...");
}

void showSettings() {
  currentScreen = 1;
  logger("showSettings()", "info-quiet");

  wm.startWebPortal();
  portalRunning = true;

  StickCP2.Display.fillScreen(TFT_BLACK);
  StickCP2.Display.setCursor(0, 0);
  StickCP2.Display.setFreeFont(FSS9);
  StickCP2.Display.setTextColor(WHITE, BLACK);
  
  StickCP2.Display.println("SSID: " + String(WiFi.SSID()));
  StickCP2.Display.println(WiFi.localIP());
  StickCP2.Display.println("Tally Arbiter Server:");
  StickCP2.Display.println(String(tallyarbiter_host) + ":" + String(tallyarbiter_port));
  StickCP2.Display.println();
  StickCP2.Display.print("Battery: ");
  
  int batteryLevel = StickCP2.Power.getBatteryLevel();
  StickCP2.Display.println(String(batteryLevel) + "%");
}

void showDeviceInfo() {
  currentScreen = 0;
  logger("showDeviceInfo()", "info-quiet");

  if (portalRunning) {
    wm.stopWebPortal();
    portalRunning = false;
  }

  StickCP2.Display.fillScreen(TFT_BLACK);
  StickCP2.Display.setCursor(4, 50);
  StickCP2.Display.setFreeFont(FSS24);
  StickCP2.Display.setTextColor(DARKGREY, BLACK);
  StickCP2.Display.println(DeviceName);

  evaluateMode();
}

void flashScreen() {
  for (int i = 0; i < 3; i++) {
    StickCP2.Display.fillScreen(WHITE);
    delay(FLASH_DELAY);
    StickCP2.Display.fillScreen(TFT_BLACK);
    delay(FLASH_DELAY);
  }

  // Resume normal operation
  if (currentScreen == 0) {
    showDeviceInfo();
  } else {
    showSettings();
  }
}

void showReassignFlash() {
  for (int i = 0; i < 2; i++) {
    StickCP2.Display.fillScreen(RED);
    delay(200);
    StickCP2.Display.fillScreen(TFT_BLACK);
    delay(200);
  }
}

void updateBrightness() {
  currentBrightness += BRIGHTNESS_INCREMENT;
  if (currentBrightness > MAX_BRIGHTNESS) {
    currentBrightness = START_BRIGHTNESS;
  }

  logger("Set brightness: " + String(currentBrightness), "info-quiet");
  StickCP2.Display.setBrightness(currentBrightness);
}

void evaluateMode() {
  if (actualType != prevType) {
    StickCP2.Display.setCursor(4, 50);
    StickCP2.Display.setFreeFont(FSS24);

    if (actualType != "") {
      // Parse color
      String hexstring = actualColor;
      hexstring.replace("#", "");
      long number = (long)strtol(&hexstring[0], NULL, 16);
      int r = number >> 16;
      int g = (number >> 8) & 0xFF;
      int b = number & 0xFF;

      StickCP2.Display.setTextColor(BLACK);
      StickCP2.Display.fillScreen(StickCP2.Display.color565(r, g, b));
      StickCP2.Display.println(DeviceName);

      logger("Device is in " + actualType + " (color " + actualColor + 
             " priority " + String(actualPriority) + ")", "info");
      Serial.println(" r: " + String(r) + " g: " + String(g) + " b: " + String(b));
    } else {
      StickCP2.Display.setTextColor(DARKGREY, BLACK);
      StickCP2.Display.fillScreen(TFT_BLACK);
      StickCP2.Display.println(DeviceName);
      logger("Device is off tally", "info");
    }

    prevType = actualType;
  }

  if (LAST_MSG && LastMessage.length() > 0) {
    StickCP2.Display.println(LastMessage);
  }
}

// ==============================================================================
// BUTTON HANDLING
// ==============================================================================

void checkPowerButton() {
  if (!StickCP2.BtnA.wasHold()) {
    return;
  }

  unsigned long holdTime = millis();
  
  if (StickCP2.BtnA.pressedFor(POWEROFF_HOLD_TIME)) {
    StickCP2.Display.fillScreen(BLACK);
    StickCP2.Display.setCursor(0, 0);
    StickCP2.Display.setTextColor(WHITE);
    StickCP2.Display.println("Power Down...");
    delay(1000);
    StickCP2.Power.powerOff();
  } else if (StickCP2.BtnA.pressedFor(RESTART_HOLD_TIME)) {
    StickCP2.Display.fillScreen(BLACK);
    StickCP2.Display.setCursor(0, 0);
    StickCP2.Display.setTextColor(WHITE);
    StickCP2.Display.println("Restarting...");
    delay(1000);
    esp_restart();
  }
}

void checkReset() {
  if (digitalRead(TRIGGER_PIN) != LOW) {
    return;
  }

  // Debounce
  delay(DEBOUNCE_DELAY);
  if (digitalRead(TRIGGER_PIN) != LOW) {
    return;
  }

  StickCP2.Display.fillScreen(TFT_BLACK);
  StickCP2.Display.setCursor(0, 40);
  StickCP2.Display.setFreeFont(FSS9);
  StickCP2.Display.setTextColor(WHITE, BLACK);
  StickCP2.Display.println("Reset button pushed....");
  logger("Button Pressed", "info");

  delay(RESET_HOLD_TIME);
  
  if (digitalRead(TRIGGER_PIN) == LOW) {
    StickCP2.Display.println("Erasing....");
    logger("Button Held", "info");
    logger("Erasing Config, restarting", "info");
    wm.resetSettings();
    ESP.restart();
  }

  StickCP2.Display.println("Starting Portal...");
  logger("Starting config portal", "info");
  wm.setConfigPortalTimeout(PORTAL_TIMEOUT);

  if (!wm.startConfigPortal(listenerDeviceName.c_str())) {
    logger("Failed to connect or hit timeout", "error");
    delay(3000);
  } else {
    logger("Connected...yay :)", "info");
  }
}

void handleButtonClicks() {
  btnM5.update();
  btnAction.update();
  StickCP2.update();

  // WiFi reset on 5-second hold
  if (StickCP2.BtnA.pressedFor(RESTART_HOLD_TIME)) {
    logger("resetSettings()", "info");
    wm.resetSettings();
    ESP.restart();
  }

  // Screen toggle
  if (btnM5.isClick()) {
    if (currentScreen == 0) {
      showSettings();
    } else {
      showDeviceInfo();
    }
  }

  // Brightness adjustment
  if (btnAction.isClick()) {
    updateBrightness();
  }
}

// ==============================================================================
// NETWORK FUNCTIONS
// ==============================================================================

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case SYSTEM_EVENT_STA_GOT_IP:
      logger("Network connected!", "info");
      logger(WiFi.localIP().toString(), "info");
      networkConnected = true;
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      logger("Network connection lost!", "info");
      networkConnected = false;
      break;
    default:
      break;
  }
}

String getParam(const String& name) {
  if (wm.server->hasArg(name)) {
    return wm.server->arg(name);
  }
  return "";
}

void saveParamCallback() {
  logger("[CALLBACK] saveParamCallback fired", "info-quiet");
  
  String str_taHost = getParam("taHostIP");
  String str_taPort = getParam("taHostPort");

  logger("Saving new TallyArbiter host: " + str_taHost, "info-quiet");
  logger("Saving new TallyArbiter port: " + str_taPort, "info-quiet");
  
  savePreference("taHost", str_taHost);
  savePreference("taPort", str_taPort);
}

void connectToNetwork() {
  WiFi.mode(WIFI_STA);
  logger("Connecting to SSID: " + String(WiFi.SSID()), "info");

  // Configure WiFiManager parameters
  custom_taServer = new WiFiManagerParameter("taHostIP", "Tally Arbiter Server", 
                                              tallyarbiter_host, 40);
  custom_taPort = new WiFiManagerParameter("taHostPort", "Port", 
                                            tallyarbiter_port, 6);

  wm.addParameter(custom_taServer);
  wm.addParameter(custom_taPort);
  wm.setSaveParamsCallback(saveParamCallback);

  // Configure menu
  std::vector<const char *> menu = { "wifi", "param", "info", "sep", "restart", "exit" };
  wm.setMenu(menu);
  wm.setClass("invert");
  wm.setConfigPortalTimeout(PORTAL_TIMEOUT);

  // Display connection status
  if (wm.getWiFiIsSaved()) {
    StickCP2.Display.println("connecting...");
  } else {
    StickCP2.Display.println("Configure on");
    StickCP2.Display.println("SSID: " + listenerDeviceName);
  }

  bool res = wm.autoConnect(listenerDeviceName.c_str());

  if (!res) {
    logger("Failed to connect", "error");
    StickCP2.Display.println("Configuration timeout");
    StickCP2.Display.println("Restart device to configure");
  } else {
    logger("Connected...yay :)", "info");
    networkConnected = true;
  }
}

// ==============================================================================
// WEBSOCKET FUNCTIONS
// ==============================================================================

void ws_emit(const String& event, const char *payload = NULL) {
  String msg;
  if (payload) {
    msg = "[\"" + event + "\"," + payload + "]";
  } else {
    msg = "[\"" + event + "\"]";
  }
  socket.sendEVENT(msg);
}

void connectToServer() {
  logger("Connecting to Tally Arbiter host: " + String(tallyarbiter_host), "info");
  socket.onEvent(socket_event);
  socket.begin(tallyarbiter_host, atol(tallyarbiter_port));
}

String strip_quot(String str) {
  if (str.length() > 0 && str[0] == '"') {
    str.remove(0, 1);
  }
  if (str.length() > 0 && str.endsWith("\"")) {
    str.remove(str.length() - 1, 1);
  }
  return str;
}

void socket_Connected(const char *payload, size_t length) {
  logger("Connected to Tally Arbiter server.", "info");
  logger("DeviceId: " + DeviceId, "info-quiet");
  
  char charDeviceObj[1024];
  snprintf(charDeviceObj, sizeof(charDeviceObj),
           "{\"deviceId\": \"%s\", \"listenerType\": \"%s\", \"canBeReassigned\": true, \"canBeFlashed\": true, \"supportsChat\": true }",
           DeviceId.c_str(), listenerDeviceName.c_str());
  
  ws_emit("listenerclient_connect", charDeviceObj);
}

void socket_Flash() {
  flashScreen();
}

void socket_Reassign(const String& payload) {
  logger("socket_Reassign()", "info-quiet");
  
  int commaIndex = payload.indexOf(',');
  if (commaIndex == -1) {
    logger("Invalid reassign payload", "error");
    return;
  }
  
  String oldDeviceId = payload.substring(0, commaIndex);
  String remaining = payload.substring(commaIndex + 1);
  int secondCommaIndex = remaining.indexOf(',');
  String newDeviceId = remaining.substring(0, secondCommaIndex);
  
  oldDeviceId = strip_quot(oldDeviceId);
  newDeviceId = strip_quot(newDeviceId);

  char charReassignObj[1024];
  snprintf(charReassignObj, sizeof(charReassignObj),
           "{\"oldDeviceId\": \"%s\", \"newDeviceId\": \"%s\"}",
           oldDeviceId.c_str(), newDeviceId.c_str());
  
  ws_emit("listener_reassign_object", charReassignObj);
  ws_emit("devices");

  showReassignFlash();

  logger("newDeviceId: " + newDeviceId, "info-quiet");
  DeviceId = newDeviceId;
  savePreference("deviceid", newDeviceId);
  SetDeviceName();
}

void socket_Messaging(const String& payload) {
  int typeQuoteIndex = payload.indexOf(',');
  if (typeQuoteIndex == -1) {
    return;
  }
  
  String messageType = payload.substring(0, typeQuoteIndex);
  messageType.replace("\"", "");
  
  int messageQuoteIndex = payload.lastIndexOf(',');
  String message = payload.substring(messageQuoteIndex + 1);
  message.replace("\"", "");
  
  LastMessage = messageType + ": " + message;
  evaluateMode();
}

void socket_event(socketIOmessageType_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case sIOtype_CONNECT:
      socket_Connected((char *)payload, length);
      break;

    case sIOtype_EVENT:
      {
        String eventMsg = (char *)payload;
        int firstQuoteEnd = eventMsg.indexOf("\"", 2);
        if (firstQuoteEnd == -1) break;
        
        String eventType = eventMsg.substring(2, firstQuoteEnd);
        String eventContent = eventMsg.substring(eventType.length() + 4);
        if (eventContent.length() > 0) {
          eventContent.remove(eventContent.length() - 1);
        }

        logger("Got event '" + eventType + "', data: " + eventContent, "info-quiet");

        if (eventType == "bus_options") {
          BusOptions = JSON.parse(eventContent);
        } else if (eventType == "reassign") {
          socket_Reassign(eventContent);
        } else if (eventType == "flash") {
          socket_Flash();
        } else if (eventType == "messaging") {
          socket_Messaging(eventContent);
        } else if (eventType == "deviceId") {
          DeviceId = eventContent.substring(1, eventContent.length() - 1);
          SetDeviceName();
          showDeviceInfo();
          currentScreen = 0;
        } else if (eventType == "devices") {
          Devices = JSON.parse(eventContent);
          SetDeviceName();
        } else if (eventType == "device_states") {
          DeviceStates = JSON.parse(eventContent);
          processTallyData();
        }
      }
      break;

    case sIOtype_DISCONNECT:
    case sIOtype_ACK:
    case sIOtype_ERROR:
    case sIOtype_BINARY_EVENT:
    case sIOtype_BINARY_ACK:
    default:
      // Not handled
      break;
  }
}

// ==============================================================================
// TALLY DATA PROCESSING
// ==============================================================================

String getBusTypeById(const String& busId) {
  for (int i = 0; i < BusOptions.length(); i++) {
    if (JSON.stringify(BusOptions[i]["id"]) == busId) {
      return JSON.stringify(BusOptions[i]["type"]);
    }
  }
  return "invalid";
}

String getBusColorById(const String& busId) {
  for (int i = 0; i < BusOptions.length(); i++) {
    if (JSON.stringify(BusOptions[i]["id"]) == busId) {
      return JSON.stringify(BusOptions[i]["color"]);
    }
  }
  return "invalid";
}

int getBusPriorityById(const String& busId) {
  for (int i = 0; i < BusOptions.length(); i++) {
    if (JSON.stringify(BusOptions[i]["id"]) == busId) {
      String priorityStr = JSON.stringify(BusOptions[i]["priority"]);
      return priorityStr.toInt();
    }
  }
  return 0;
}

void processTallyData() {
  bool typeChanged = false;
  
  for (int i = 0; i < DeviceStates.length(); i++) {
    if (DeviceStates[i]["sources"].length() > 0) {
      typeChanged = true;
      actualType = getBusTypeById(JSON.stringify(DeviceStates[i]["busId"]));
      actualColor = getBusColorById(JSON.stringify(DeviceStates[i]["busId"]));
      actualPriority = getBusPriorityById(JSON.stringify(DeviceStates[i]["busId"]));
    }
  }

  if (!typeChanged) {
    actualType = "";
    actualColor = "";
    actualPriority = 0;
  }
  
  evaluateMode();
}

void SetDeviceName() {
  for (int i = 0; i < Devices.length(); i++) {
    if (JSON.stringify(Devices[i]["id"]) == "\"" + DeviceId + "\"") {
      String strDevice = JSON.stringify(Devices[i]["name"]);
      DeviceName = strDevice.substring(1, strDevice.length() - 1);
      break;
    }
  }
  
  savePreference("devicename", DeviceName);
  evaluateMode();
  logger("DeviceName: " + DeviceName, "info");
}

// ==============================================================================
// OTA UPDATE CONFIGURATION
// ==============================================================================

void setupOTA() {
  ArduinoOTA.setHostname(listenerDeviceName.c_str());
  ArduinoOTA.setPassword("tallyarbiter");
  
  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    Serial.println("Start updating " + type);
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) logger("Auth Failed", "error");
    else if (error == OTA_BEGIN_ERROR) logger("Begin Failed", "error");
    else if (error == OTA_CONNECT_ERROR) logger("Connect Failed", "error");
    else if (error == OTA_RECEIVE_ERROR) logger("Receive Failed", "error");
    else if (error == OTA_END_ERROR) logger("End Failed", "error");
  });

  ArduinoOTA.begin();
}

// ==============================================================================
// SETUP AND LOOP
// ==============================================================================

void setup() {
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  Serial.begin(115200);
  StickCP2.Power.begin();
  
  while (!Serial);

  logger("Initializing M5StickCPlus2.", "info-quiet");

  // Power saving
  setCpuFrequencyMhz(80);
  btStop();

  // Generate unique device name
  byte mac[6];
  WiFi.macAddress(mac);
  listenerDeviceName = listenerDeviceName + 
                       String(mac[3], HEX) + 
                       String(mac[4], HEX) + 
                       String(mac[5], HEX);

  wm.setHostname(listenerDeviceName.c_str());
  
  // Initialize M5StickC
  auto cfg = M5.config();
  StickCP2.begin(cfg);
  
  initializeDisplay();
  showBootScreen();

  logger("Tally Arbiter M5StickC Listener Client booting.", "info");
  logger("Listener device name: " + listenerDeviceName, "info");

  // Load preferences
  loadAllPreferences();

  delay(100);
  connectToNetwork();
  
  while (!networkConnected) {
    delay(200);
  }

  setupOTA();
  connectToServer();
  showSettings();
}

void loop() {
  if (portalRunning) {
    wm.process();
  }

  checkReset();
  checkPowerButton();
  ArduinoOTA.handle();
  socket.loop();
  handleButtonClicks();
}
