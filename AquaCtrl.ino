#include <WiFi.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C
#define OLED_RESET -1
#define EEPROM_ADDR 0

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
String jsonRlyInfo;  // Global variable for relay information JSON

// WiFi configuration with multiple options
const int NUM_NETWORKS = 2;
const char* ssids[NUM_NETWORKS] = {"Wi-fi", "Backup_Network"};
const char* passwords[NUM_NETWORKS] = {"Wi-fi-senha", "backup_password"};

// NTP server configuration with more reliable settings
const char* ntpServer = "pool.ntp.org";
const char* ntpServerBackup = "time.google.com";
const long gmtOffset_sec = -10800;  // -3 hours in seconds
const int daylightOffset_sec = 0;   // No daylight saving adjustment

// EEPROM save delay configuration
unsigned long saveToEEPROMStartTime = 0;
const unsigned long saveToEEPROMDelay = 3000;  // 3 seconds delay

// Web server setup
AsyncWebServer server(80);

// Menu navigation variables
int highlightedField = 1;
float sizeoftext = 1;
bool isMenu1Active = true;
int currentSelectedRly = 0;
int currentEditingField = 1;
unsigned long lastKeyPressTime = 0;

// Keypad configuration
const int ROWS = 2;
const int COLS = 4;
int rowPins[ROWS] = {12, 14};
int colPins[COLS] = {26, 27, 33, 25};
char keys[ROWS][COLS] = {
  {'L', 'R', 'U', 'D'},
  {'1', '2', '3', '4'}
};

// Relay configuration
const int numRelays = 8;
int relayPins[numRelays] = {15, 2, 4, 16, 17, 5, 18, 19};

// Matrix to store relay information
// [0] = Relay number
// [1] = ON Hour
// [2] = ON Minute
// [3] = OFF Hour
// [4] = OFF Minute
// [5] = Use schedule (1 = yes, 0 = no)
// [6] = Current state (1 = on, 0 = off)
int rlyInfo[7][numRelays] = {
  {1, 2, 3, 4, 5, 6, 7, 8},          // Relay number
  {23, 23, 18, 18, 18, 18, 18, 18},  // ON Hour
  {04, 03, 0, 0, 0, 0, 0, 0},        // ON Minute
  {19, 19, 19, 19, 19, 19, 19, 19},  // OFF Hour
  {0, 0, 0, 0, 0, 0, 0, 0},          // OFF Minute
  {1, 0, 1, 0, 0, 1, 1, 0},          // Use schedule (1 = yes, 0 = no)
  {0, 0, 0, 0, 0, 0, 0, 0}           // Current state (1 = on, 0 = off)
};

void setup() {
  Serial.begin(115200);
  
  // Initialize display
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(sizeoftext);
  display.setCursor(0, 0);
  display.println("Starting...");
  display.display();
  
  // Connect to WiFi with multiple options
  connectToWiFi();
  
  // Configure time with improved reliability
  configureTime();
  
  // Configure keypad pins
  for (int i = 0; i < COLS; i++) {
    pinMode(colPins[i], INPUT_PULLUP);
  }
  
  // Configure relay pins and initialize all to OFF
  for (int i = 0; i < numRelays; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH);  // Relays are active LOW
  }
  
  // Load saved settings from EEPROM
  loadFromEEPROM();
  
  // Set up web server routes
  setupWebServer();
  server.begin();
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("System ready");
  display.println("IP: " + WiFi.localIP().toString());
  display.display();
  delay(2000);
}

void loop() {
  unsigned long currentTime = millis();

  // Auto return to main menu after 10 seconds of inactivity
  if (currentTime - lastKeyPressTime >= 10000) {
    isMenu1Active = true;
  }

  // Read keypad input
  char key = readKeypad();
  if (key != 0) {
    lastKeyPressTime = currentTime;
    Serial.print("Key pressed: ");
    Serial.println(key);

    // Menu navigation
    if (isMenu1Active && (key == 'R')) {
      isMenu1Active = false;
      currentSelectedRly = 0;
      currentEditingField = 1;
    }
    
    // Save to EEPROM when 'L' is held in menu1
    if (isMenu1Active && (key == 'L')) {
      if (currentTime - saveToEEPROMStartTime >= saveToEEPROMDelay) {
        saveToEEPROM();
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("Saving to EEPROM...");
        display.display();
        delay(1000);
        saveToEEPROMStartTime = currentTime;
      }
    }
    
    // Update highlighted field based on key press
    updateHighlight(key);

    // Toggle relay states with number buttons in menu1
    if (isMenu1Active && (key == '1' || key == '2' || key == '3' || key == '4')) {
      int buttonIndex = key - '1';
      if (buttonIndex >= 0 && buttonIndex < numRelays) {
        rlyInfo[6][buttonIndex] = !rlyInfo[6][buttonIndex];
        digitalWrite(relayPins[buttonIndex], rlyInfo[6][buttonIndex] ? LOW : HIGH);
      }
    }
  }

  // Update display based on active menu
  display.clearDisplay();
  if (isMenu1Active) {
    drawMenu1();
  } else {
    drawMenu2();
  }
  display.display();

  // Update relay states based on schedule
  updateRelays();

  // Update JSON when relay info changes
  String newJsonRlyInfo = generateJsonFromRlyInfo();
  if (newJsonRlyInfo != jsonRlyInfo) {
    jsonRlyInfo = newJsonRlyInfo;
    Serial.println("JSON RlyInfo updated: ");
    Serial.println(jsonRlyInfo);
  }

  // Periodically check and refresh time sync
  static unsigned long lastTimeSync = 0;
  if (currentTime - lastTimeSync > 3600000) { // Every hour
    configureTime();
    lastTimeSync = currentTime;
  }

  delay(100);
}

// Connect to one of the configured WiFi networks
void connectToWiFi() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Connecting to WiFi...");
  display.display();
  
  bool connected = false;
  
  for (int i = 0; i < NUM_NETWORKS; i++) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Trying: ");
    display.println(ssids[i]);
    display.display();
    
    Serial.print("Connecting to ");
    Serial.println(ssids[i]);
    
    WiFi.begin(ssids[i], passwords[i]);
    
    // Try to connect for 10 seconds
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      display.print(".");
      display.display();
      attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
      Serial.println("\nWiFi connected");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("WiFi connected!");
      display.print("IP: ");
      display.println(WiFi.localIP().toString());
      display.display();
      delay(2000);
      break;
    } else {
      Serial.println("\nFailed to connect to " + String(ssids[i]));
      WiFi.disconnect();
    }
  }
  
  if (!connected) {
    Serial.println("Failed to connect to any WiFi network");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi connection failed");
    display.println("Check settings");
    display.display();
    delay(5000);
  }
}

// Configure NTP time with improved reliability
void configureTime() {
  // Try primary NTP server
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Wait for time to be set
  int retry = 0;
  time_t now;
  struct tm timeinfo;
  
  while (!getLocalTime(&timeinfo) && retry < 5) {
    Serial.println("Failed to obtain time from primary NTP server");
    delay(1000);
    retry++;
  }
  
  // If primary server failed, try backup
  if (retry >= 5) {
    Serial.println("Trying backup NTP server");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServerBackup);
    
    retry = 0;
    while (!getLocalTime(&timeinfo) && retry < 5) {
      Serial.println("Failed to obtain time from backup NTP server");
      delay(1000);
      retry++;
    }
  }
  
  if (getLocalTime(&timeinfo)) {
    Serial.println("Time synchronized");
    char timeString[30];
    strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
    Serial.print("Current time: ");
    Serial.println(timeString);
  } else {
    Serial.println("Time synchronization failed");
  }
}

// Draw main screen (Menu 1)
void drawMenu1() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    display.setCursor(0, 0);
    display.println("Time sync error");
    return;
  }

  char currentTime[20];
  sprintf(currentTime, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

  display.setCursor(0, 0);
  display.print("Current time:");
  display.setCursor(0, 8);
  display.print(currentTime);

  for (int i = 0; i < numRelays; i += 2) {
    int rlyNumber1 = rlyInfo[0][i];
    bool currentState1 = rlyInfo[6][i];
    int hourOn1 = rlyInfo[1][i];
    int minuteOn1 = rlyInfo[2][i];
    int hourOff1 = rlyInfo[3][i];
    int minuteOff1 = rlyInfo[4][i];
    bool useSchedule1 = rlyInfo[5][i];

    int rlyNumber2 = i + 1 < numRelays ? rlyInfo[0][i + 1] : 0;
    bool currentState2 = i + 1 < numRelays ? rlyInfo[6][i + 1] : false;
    int hourOn2 = i + 1 < numRelays ? rlyInfo[1][i + 1] : 0;
    int minuteOn2 = i + 1 < numRelays ? rlyInfo[2][i + 1] : 0;
    int hourOff2 = i + 1 < numRelays ? rlyInfo[3][i + 1] : 0;
    int minuteOff2 = i + 1 < numRelays ? rlyInfo[4][i + 1] : 0;
    bool useSchedule2 = i + 1 < numRelays ? rlyInfo[5][i + 1] : false;

    char rlyNumberStr1[8];
    sprintf(rlyNumberStr1, "R%d_", rlyNumber1);

    display.setCursor(0, 20 + i * 5);

    // Draw a line over the text if "use schedule" is off
    if (!useSchedule1) {
      drawLineInFirstColumn(24 + i * 5);
    }

    display.print(rlyNumberStr1);
    display.print(currentState1 ? "O" : "X");
    display.print(" ");

    int currentMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;

    // Calculate the remaining time to the next change in state for relay 1
    int remainingMinutes1;
    if (currentState1) {
      remainingMinutes1 = hourOff1 * 60 + minuteOff1 - currentMinutes;
    } else {
      remainingMinutes1 = hourOn1 * 60 + minuteOn1 - currentMinutes;
    }

    // Normalize the remaining time to handle the case when it crosses midnight
    if (remainingMinutes1 < 0) {
      remainingMinutes1 += 24 * 60;
    }

    int remainingHours1 = remainingMinutes1 / 60;
    remainingMinutes1 %= 60;

    // Display the remaining time with two digits for both hours and minutes
    display.printf("%02d:%02d", remainingHours1, remainingMinutes1);
    display.print("|");

    // Only show second relay if it exists
    if (i + 1 < numRelays) {
      char rlyNumberStr2[8];
      sprintf(rlyNumberStr2, "R%d_", rlyNumber2);
      
      // Draw a line over the text if "use schedule" is off
      if (!useSchedule2) {
        drawLineInSecondColumn(24 + i * 5);
      }
      
      display.print(rlyNumberStr2);
      display.print(currentState2 ? "O" : "X");
      display.print(" ");
      
      // Calculate the remaining time to the next change in state for relay 2
      int remainingMinutes2;
      if (currentState2) {
        remainingMinutes2 = hourOff2 * 60 + minuteOff2 - currentMinutes;
      } else {
        remainingMinutes2 = hourOn2 * 60 + minuteOn2 - currentMinutes;
      }
      
      // Normalize the remaining time to handle the case when it crosses midnight
      if (remainingMinutes2 < 0) {
        remainingMinutes2 += 24 * 60;
      }
      
      int remainingHours2 = remainingMinutes2 / 60;
      remainingMinutes2 %= 60;
      
      // Display the remaining time with two digits for both hours and minutes
      display.printf("%02d:%02d", remainingHours2, remainingMinutes2);
    }
  }
}

// Draw configuration screen (Menu 2)
void drawMenu2() {
  for (int i = 0; i < numRelays; i++) {
    bool isHighlighted = (i == currentSelectedRly);

    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, i * 8);
    display.print("R");
    display.print(rlyInfo[0][i]);
    display.print(" ");

    char hourOnStr[3];
    sprintf(hourOnStr, "%02d", rlyInfo[1][i]);
    drawTextWithHighlight(display, isHighlighted, 1, hourOnStr);
    display.print(":");

    char minuteOnStr[3];
    sprintf(minuteOnStr, "%02d", rlyInfo[2][i]);
    drawTextWithHighlight(display, isHighlighted, 2, minuteOnStr);
    display.print(" | ");

    char hourOffStr[3];
    sprintf(hourOffStr, "%02d", rlyInfo[3][i]);
    drawTextWithHighlight(display, isHighlighted, 3, hourOffStr);
    display.print(":");

    char minuteOffStr[3];
    sprintf(minuteOffStr, "%02d", rlyInfo[4][i]);
    drawTextWithHighlight(display, isHighlighted, 4, minuteOffStr);
    display.print(" ");

    drawTextWithHighlight(display, isHighlighted, 5, rlyInfo[5][i] ? "O" : "X");
    display.print(" ");

    drawTextWithHighlight(display, isHighlighted, 6, rlyInfo[6][i] ? "O" : "X");
  }
}

// Draw text with highlighting for selected fields
void drawTextWithHighlight(Adafruit_SSD1306& display, bool isHighlighted, int field, const char* text) {
  if (isHighlighted && field == highlightedField) {
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);  // Black text with white background
  } else {
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);  // White text with black background
  }
  display.print(text);
  display.setTextColor(SSD1306_WHITE);  // Restore default color
}

// Draw horizontal line in first column
void drawLineInFirstColumn(int16_t y) {
  display.drawLine(0, y - 2, SCREEN_WIDTH / 2, y - 2, SSD1306_WHITE);
}

// Draw horizontal line in second column
void drawLineInSecondColumn(int16_t y) {
  display.drawLine(SCREEN_WIDTH / 2, y - 2, SCREEN_WIDTH, y - 2, SSD1306_WHITE);
}

// Read keypad input
char readKeypad() {
  // Matrix of characters for the keypad
  char keymap[ROWS][COLS] = {
    {'L', 'R', 'U', 'D'},
    {'3', '4', '2', '1'}
  };

  // Set rows as OUTPUT and columns as INPUT_PULLUP
  for (int i = 0; i < ROWS; i++) {
    pinMode(rowPins[i], OUTPUT);
    digitalWrite(rowPins[i], LOW);
  }

  for (int j = 0; j < COLS; j++) {
    pinMode(colPins[j], INPUT_PULLUP);
  }

  // Check for pressed keys
  char key = 0;
  for (int i = 0; i < ROWS; i++) {
    digitalWrite(rowPins[i], HIGH);
    for (int j = 0; j < COLS; j++) {
      if (digitalRead(colPins[j]) == LOW) {
        key = keymap[i][j];
        break;
      }
    }
    digitalWrite(rowPins[i], LOW);
    if (key != 0) break;
  }

  return key;
}

// Update relay states based on schedule
void updateRelays() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return;
  }

  int currentHour = timeinfo.tm_hour;
  int currentMinute = timeinfo.tm_min;
  int currentSecond = timeinfo.tm_sec;

  for (int i = 0; i < numRelays; i++) {
    int hourOn = rlyInfo[1][i];
    int minuteOn = rlyInfo[2][i];
    int hourOff = rlyInfo[3][i];
    int minuteOff = rlyInfo[4][i];
    bool useSchedule = rlyInfo[5][i];
    bool currentState = rlyInfo[6][i];

    // Check if relay is scheduled to turn ON and useSchedule is enabled
    if (useSchedule && currentHour == hourOn && currentMinute == minuteOn && currentSecond < 2) {
      currentState = 1;
    }
    // Check if relay is scheduled to turn OFF and useSchedule is enabled
    if (useSchedule && currentHour == hourOff && currentMinute == minuteOff && currentSecond < 2) {
      currentState = 0;
    }

    // Update the state in the matrix
    rlyInfo[6][i] = currentState;

    // Update the corresponding relay
    digitalWrite(relayPins[i], currentState ? LOW : HIGH);  // Relays are active LOW
  }
}

// Update highlighted field based on keypad input
void updateHighlight(char key) {
  if (isMenu1Active) {
    // In Menu 1, U/D keys select the current relay
    if (key == 'U') {
      currentSelectedRly = (currentSelectedRly - 1 + numRelays) % numRelays;
    } else if (key == 'D') {
      currentSelectedRly = (currentSelectedRly + 1) % numRelays;
    }
  } else {
    // In Menu 2, L/R keys navigate between fields, U/D keys change values
    if (key == 'L') {
      // Navigate to previous field or previous relay
      if (currentEditingField == 1) {
        // If at first field, go to previous relay's last field
        currentSelectedRly = (currentSelectedRly - 1 + numRelays) % numRelays;
        currentEditingField = 6;
      } else {
        // Otherwise go to previous field
        currentEditingField--;
      }
    } else if (key == 'R') {
      // Navigate to next field or next relay
      if (currentEditingField == 6) {
        // If at last field, go to next relay's first field
        currentSelectedRly = (currentSelectedRly + 1) % numRelays;
        currentEditingField = 1;
      } else {
        // Otherwise go to next field
        currentEditingField++;
      }
    } else if (key == 'U') {
      // Increase the value of the current editing field
      switch (currentEditingField) {
        case 1: // Hour ON
          rlyInfo[1][currentSelectedRly] = (rlyInfo[1][currentSelectedRly] + 1) % 24;
          break;
        case 2: // Minute ON
          rlyInfo[2][currentSelectedRly] = (rlyInfo[2][currentSelectedRly] + 1) % 60;
          break;
        case 3: // Hour OFF
          rlyInfo[3][currentSelectedRly] = (rlyInfo[3][currentSelectedRly] + 1) % 24;
          break;
        case 4: // Minute OFF
          rlyInfo[4][currentSelectedRly] = (rlyInfo[4][currentSelectedRly] + 1) % 60;
          break;
        case 5: // Use schedule
          rlyInfo[5][currentSelectedRly] = !rlyInfo[5][currentSelectedRly];
          break;
        case 6: // Current state
          rlyInfo[6][currentSelectedRly] = !rlyInfo[6][currentSelectedRly];
          digitalWrite(relayPins[currentSelectedRly], rlyInfo[6][currentSelectedRly] ? LOW : HIGH);
          break;
      }
    } else if (key == 'D') {
      // Decrease the value of the current editing field
      switch (currentEditingField) {
        case 1: // Hour ON
          rlyInfo[1][currentSelectedRly] = (rlyInfo[1][currentSelectedRly] + 23) % 24;
          break;
        case 2: // Minute ON
          rlyInfo[2][currentSelectedRly] = (rlyInfo[2][currentSelectedRly] + 59) % 60;
          break;
        case 3: // Hour OFF
          rlyInfo[3][currentSelectedRly] = (rlyInfo[3][currentSelectedRly] + 23) % 24;
          break;
        case 4: // Minute OFF
          rlyInfo[4][currentSelectedRly] = (rlyInfo[4][currentSelectedRly] + 59) % 60;
          break;
        case 5: // Use schedule
          rlyInfo[5][currentSelectedRly] = !rlyInfo[5][currentSelectedRly];
          break;
        case 6: // Current state
          rlyInfo[6][currentSelectedRly] = !rlyInfo[6][currentSelectedRly];
          digitalWrite(relayPins[currentSelectedRly], rlyInfo[6][currentSelectedRly] ? LOW : HIGH);
          break;
      }
    }
    // Update highlighted field
    highlightedField = currentEditingField;
  }
}

// Set up web server routes
void setupWebServer() {
  // Serve the main page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = generateHtmlPage();
    request->send(200, "text/html", html);
  });
  
  // API to get all relay information as JSON
  server.on("/api/relays", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = generateJsonFromRlyInfo();
    request->send(200, "application/json", json);
  });
  
  // API to update a specific relay setting
  server.on("/api/update", HTTP_GET, [](AsyncWebServerRequest *request) {
    String message = "Update failed";
    bool success = false;
    
    if (request->hasParam("relay") && request->hasParam("field") && request->hasParam("value")) {
      int relayIndex = request->getParam("relay")->value().toInt() - 1;
      int field = request->getParam("field")->value().toInt();
      int value = request->getParam("value")->value().toInt();
      
      if (relayIndex >= 0 && relayIndex < numRelays && field >= 1 && field <= 6) {
        rlyInfo[field][relayIndex] = value;
        
        // If updating current state, also update the physical relay
        if (field == 6) {
          digitalWrite(relayPins[relayIndex], value ? LOW : HIGH);
        }
        
        message = "Relay " + String(relayIndex + 1) + " field " + String(field) + " updated to " + String(value);
        success = true;
      }
    }
    
    request->send(200, "text/plain", message);
  });
  
  // API to save to EEPROM
  server.on("/api/save", HTTP_GET, [](AsyncWebServerRequest *request) {
    saveToEEPROM();
    request->send(200, "text/plain", "Settings saved to EEPROM");
  });
  
  // Handle 404
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
  });
}

// Generate JSON representation of relay information
String generateJsonFromRlyInfo() {
  DynamicJsonDocument doc(2048);
  JsonArray array = doc.to<JsonArray>();
  
  for (int i = 0; i < numRelays; i++) {
    JsonObject relayObj = array.createNestedObject();
    relayObj["relay"] = rlyInfo[0][i];
    relayObj["hourOn"] = rlyInfo[1][i];
    relayObj["minuteOn"] = rlyInfo[2][i];
    relayObj["hourOff"] = rlyInfo[3][i];
    relayObj["minuteOff"] = rlyInfo[4][i];
    relayObj["useSchedule"] = rlyInfo[5][i];
    relayObj["currentState"] = rlyInfo[6][i];
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  return jsonString;
}

// Save settings to EEPROM
void saveToEEPROM() {
  EEPROM.begin(sizeof(rlyInfo));
  EEPROM.put(EEPROM_ADDR, rlyInfo);
  EEPROM.commit();
  EEPROM.end();
  Serial.println("Data saved to EEPROM");
  
  // Display save confirmation
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Settings saved!");
  display.display();
  delay(1000);
}

// Load settings from EEPROM
void loadFromEEPROM() {
  EEPROM.begin(sizeof(rlyInfo));
  EEPROM.get(EEPROM_ADDR, rlyInfo);
  EEPROM.end();
  Serial.println("Data loaded from EEPROM");
  
  // Update relay states
  for (int i = 0; i < numRelays; i++) {
    digitalWrite(relayPins[i], rlyInfo[6][i] ? LOW : HIGH);
  }
}

// Generate HTML for the web interface
String generateHtmlPage() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Relay Control System</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background-color: #f0f0f0; }";
  html += ".container { max-width: 800px; margin: 0 auto; background-color: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }";
  html += "h1 { color: #333; }";
  html += "table { width: 100%; border-collapse: collapse; margin: 20px 0; }";
  html += "th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }";
  html += "th { background-color: #f2f2f2; }";
  html += "tr:nth-child(even) { background-color: #f9f9f9; }";
  html += ".toggle { position: relative; display: inline-block; width: 60px; height: 34px; }";
  html += ".toggle input { opacity: 0; width: 0; height: 0; }";
  html += ".slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; transition: .4s; border-radius: 34px; }";
  html += ".slider:before { position: absolute; content: ''; height: 26px; width: 26px; left: 4px; bottom: 4px; background-color: white; transition: .4s; border-radius: 50%; }";
  html += "input:checked + .slider { background-color: #2196F3; }";
  html += "input:checked + .slider:before { transform: translateX(26px); }";
  html += ".btn { background-color: #4CAF50; color: white; padding: 10px 15px; border: none; border-radius: 4px; cursor: pointer; margin: 5px; }";
  html += ".btn:hover { background-color: #45a049; }";
  html += ".btn-danger { background-color: #f44336; }";
  html += ".btn-danger:hover { background-color: #d32f2f; }";
  html += ".btn-primary { background-color: #2196F3; }";
  html += ".btn-primary:hover { background-color: #0b7dda; }";
  html += ".time-input { width: 50px; }";
  html += "</style>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>Relay Control System</h1>";
  html += "<p>Current local time: <span id='current-time'>Loading...</span></p>";
  html += "<div>";
  html += "<button class='btn btn-primary' onclick='refreshData()'>Refresh Data</button>";
  html += "<button class='btn' onclick='saveSettings()'>Save to EEPROM</button>";
  html += "</div>";
  
  html += "<table id='relay-table'>";
  html += "<tr><th>Relay</th><th>ON Time</th><th>OFF Time</th><th>Use Schedule</th><th>Current State</th></tr>";
  html += "</table>";
  
  html += "<script>";
  html += "function updateClock() {";
  html += "  const now = new Date();";
  html += "  document.getElementById('current-time').textContent = now.toLocaleTimeString();";
  html += "}";
  html += "setInterval(updateClock, 1000);";
  html += "updateClock();";
  
  html += "function refreshData() {";
  html += "  fetch('/api/relays')";
  html += "    .then(response => response.json())";
  html += "    .then(data => {";
  html += "      const table = document.getElementById('relay-table');";
  html += "      // Keep the header row";
  html += "      while (table.rows.length > 1) {";
  html += "        table.deleteRow(1);";
  html += "      }";
  html += "      data.forEach(relay => {";
  html += "        const row = table.insertRow();";
  html += "        // Relay number";
  html += "        const cellRelay = row.insertCell();";
  html += "        cellRelay.textContent = relay.relay;";
  
  html += "        // ON Time";
  html += "        const cellOn = row.insertCell();";
  html += "        const hourOnInput = document.createElement('input');";
  html += "        hourOnInput.type = 'number';";
  html += "        hourOnInput.className = 'time-input';";
  html += "        hourOnInput.min = 0;";
  html += "        hourOnInput.max = 23;";
  html += "        hourOnInput.value = relay.hourOn;";
  html += "        hourOnInput.onchange = function() { updateRelay(relay.relay, 1, this.value); };";
  html += "        cellOn.appendChild(hourOnInput);";
  
  html += "        cellOn.appendChild(document.createTextNode(':'));";
  
  html += "        const minuteOnInput = document.createElement('input');";
  html += "        minuteOnInput.type = 'number';";
  html += "        minuteOnInput.className = 'time-input';";
  html += "        minuteOnInput.min = 0;";
  html += "        minuteOnInput.max = 59;";
  html += "        minuteOnInput.value = relay.minuteOn;";
  html += "        minuteOnInput.onchange = function() { updateRelay(relay.relay, 2, this.value); };";
  html += "        cellOn.appendChild(minuteOnInput);";
  
  html += "        // OFF Time";
  html += "        const cellOff = row.insertCell();";
  html += "        const hourOffInput = document.createElement('input');";
  html += "        hourOffInput.type = 'number';";
  html += "        hourOffInput.className = 'time-input';";
  html += "        hourOffInput.min = 0;";
  html += "        hourOffInput.max = 23;";
  html += "        hourOffInput.value = relay.hourOff;";
  html += "        hourOffInput.onchange = function() { updateRelay(relay.relay, 3, this.value); };";
  html += "        cellOff.appendChild(hourOffInput);";
  
  html += "        cellOff.appendChild(document.createTextNode(':'));";
  
  html += "        const minuteOffInput = document.createElement('input');";
  html += "        minuteOffInput.type = 'number';";
  html += "        minuteOffInput.className = 'time-input';";
  html += "        minuteOffInput.min = 0;";
  html += "        minuteOffInput.max = 59;";
  html += "        minuteOffInput.value = relay.minuteOff;";
  html += "        minuteOffInput.onchange = function() { updateRelay(relay.relay, 4, this.value); };";
  html += "        cellOff.appendChild(minuteOffInput);";
  
  html += "        // Use Schedule";
  html += "        const cellUseSchedule = row.insertCell();";
  html += "        const useScheduleCheckbox = document.createElement('input');";
  html += "        useScheduleCheckbox.type = 'checkbox';";
  html += "        useScheduleCheckbox.checked = relay.useSchedule === 1;";
  html += "        useScheduleCheckbox.onchange = function() { updateRelay(relay.relay, 5, this.checked ? 1 : 0); };";
  html += "        cellUseSchedule.appendChild(useScheduleCheckbox);";
  
  html += "        // Current State";
  html += "        const cellState = row.insertCell();";
  html += "        const label = document.createElement('label');";
  html += "        label.className = 'toggle';";
  html += "        const stateToggle = document.createElement('input');";
  html += "        stateToggle.type = 'checkbox';";
  html += "        stateToggle.checked = relay.currentState === 1;";
  html += "        stateToggle.onchange = function() { updateRelay(relay.relay, 6, this.checked ? 1 : 0); };";
  html += "        label.appendChild(stateToggle);";
  html += "        const slider = document.createElement('span');";
  html += "        slider.className = 'slider';";
  html += "        label.appendChild(slider);";
  html += "        cellState.appendChild(label);";
  html += "      });";
  html += "    });";
  html += "}";
  
  html += "function updateRelay(relay, field, value) {";
  html += "  fetch(`/api/update?relay=${relay}&field=${field}&value=${value}`);";
  html += "}";
  
  html += "function saveSettings() {";
  html += "  fetch('/api/save')";
  html += "    .then(response => response.text())";
  html += "    .then(text => alert(text));";
  html += "}";
  
  html += "// Initial data load";
  html += "refreshData();";
  html += "</script>";
  html += "</div></body></html>";
  
  return html;
}
