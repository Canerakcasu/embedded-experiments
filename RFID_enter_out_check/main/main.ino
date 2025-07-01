#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <SD.h>
#include <MFRC522.h>
#include <map>
#include "time.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <EEPROM.h>

TaskHandle_t Task_Network_Handle;
TaskHandle_t Task_RFID_Handle;
SemaphoreHandle_t sdMutex;

#define RECONNECT_INTERVAL 60000
#define RECOVERY_MODE_TIMEOUT 300000
unsigned long lastReconnectAttempt = 0;
bool isConnected = false;

#define EEPROM_SIZE 128
String ADMIN_USER = "admin";
String ADMIN_PASS = "1234";
const char* ADD_USER_AUTH_USER = "user";
String ADD_USER_PASS = "123";

#define BEEP_FREQUENCY 2600
SPIClass hspi(HSPI);
#define HSPI_SCK_PIN      25
#define HSPI_MISO_PIN     26
#define HSPI_MOSI_PIN     27
#define SD_CS_PIN         5
#define SS_PIN            15
#define RST_PIN           22

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7200;
const int   daylightOffset_sec = 0;

#define BUZZER_PIN 32
#define CARD_COOLDOWN_SECONDS 5
#define EVENT_TIMEOUT_MS 3000
#define MESSAGE_DISPLAY_MS 5000

const int LCD_I2C_ADDR = 0x23;
const int LCD_COLS = 16;
const int LCD_ROWS = 2;
const int I2C_SDA_PIN = 13;
const int I2C_SCL_PIN = 14;

enum DisplayState { SHOWING_MESSAGE, SHOWING_TIME };
DisplayState currentDisplayState = SHOWING_TIME;
unsigned long lastCardActivityTime = 0;

#define USER_DATABASE_FILE    "/users.csv"
#define LOGS_DIRECTORY        "/logs"
#define INVALID_LOGS_FILE     "/invalid_logs.csv"
#define G_SHEETS_QUEUE_FILE   "/upload_queue.csv"
#define G_SHEETS_SENDING_FILE "/upload_sending.csv"
#define TEMP_USER_FILE        "/temp_users.csv"
const char* GOOGLE_SCRIPT_ID = "https://script.google.com/macros/s/AKfycby1vGdWeMxtKsf0mf4i98NEv_NmrrHkRSRZ5-IMXtgSmRvFoyPZToPoie28pv-td8SnkQ/exec";
#define UPLOAD_INTERVAL_MS 60000

LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);
MFRC522 rfid(SS_PIN, RST_PIN);
WebServer server(80);
WiFiManager wifiManager;

std::map<String, String> userDatabase;
std::map<String, bool> userStatus;
std::map<String, unsigned long> lastScanTime;
std::map<String, time_t> entryTime;
String lastEventUID = "N/A", lastEventName = "-", lastEventAction = "-", lastEventTime = "-";
int lastDay = -1;
unsigned long lastEventTimer = 0;

void readStringFromEEPROM(int addrOffset, char* buffer, int bufSize);
void writeStringToEEPROM(int addrOffset, const String& str);
void loadPasswordsFromEEPROM();
void setupOTA();
void loadUsersFromSd();
void playBuzzer(int status);
String getFormattedTime(time_t timestamp);
String getUserName(String uid);
String getUIDString(MFRC522::Uid uid);
void logActivityToSd(String event, String uid, String name, unsigned long duration, time_t event_time);
void logInvalidAttemptToSd(String uid);
String formatDuration(unsigned long totalSeconds);
void checkForDailyReset();
void sendDataToGoogleSheets();
void setupInitialWifi();
void setupTime();
void updateDisplayMessage(String line1, String line2);
void updateDisplayDateTime();
void syncUserListToSheets();
void Task_Network(void *pvParameters);
void Task_RFID(void *pvParameters);
void newConfigSavedCallback();
void listFiles(String& html, const char* dirname, int levels);
void handleRoot();
void handleCSS();
void handleData();
void handleAdmin();
void handleLogs();
void handleAddUserPage();
void handleGetLastUID();
void handleReboot();
void handleChangePassword();
void handleChangeAddUserPassword();
void handleFileDownload();
void handleUpdate();
void handleUpdateUpload();
void handleUpdatePage();
void handleAddUser();
void handleDeleteUser();
void handleNotFound();


void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(BUZZER_PIN, OUTPUT);
  Serial.println("\n-- RFID Access System - v3.1 (Robust WiFi) --");

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("System Booting...");

  EEPROM.begin(EEPROM_SIZE);
  loadPasswordsFromEEPROM();
  
  sdMutex = xSemaphoreCreateMutex();
  hspi.begin(HSPI_SCK_PIN, HSPI_MISO_PIN, HSPI_MOSI_PIN);
  
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  if (!SD.begin(SD_CS_PIN, hspi)) {
    Serial.println("SD Card Mount Failed!");
    updateDisplayMessage("SD Card Error", "");
    while(1);
  }
  xSemaphoreGive(sdMutex);

  rfid.PCD_Init();
  loadUsersFromSd();

  xTaskCreatePinnedToCore(Task_Network, "Network_Task", 16000, NULL, 1, &Task_Network_Handle, 1);
  xTaskCreatePinnedToCore(Task_RFID,    "RFID_Task",    8000,  NULL, 2, &Task_RFID_Handle,    0);

  Serial.println("Tasks created. System running...");
}

void loop() {
  vTaskDelete(NULL); 
}

void Task_Network(void *pvParameters) {
  Serial.println("Network Task started on Core 1.");
  
  setupInitialWifi();
  
  if (WiFi.status() == WL_CONNECTED) {
    isConnected = true;
    setupTime();
    setupOTA();
    
    server.on("/", HTTP_GET, handleRoot);
    server.on("/data", HTTP_GET, handleData);
    server.on("/admin", HTTP_GET, handleAdmin);
    server.on("/logs", HTTP_GET, handleLogs);
    server.on("/style.css", HTTP_GET, handleCSS);
    server.on("/adduserpage", HTTP_GET, handleAddUserPage);
    server.on("/getlastuid", HTTP_GET, handleGetLastUID);
    server.on("/reboot", HTTP_POST, handleReboot);
    server.on("/changepass", HTTP_POST, handleChangePassword);
    server.on("/changeadduserpass", HTTP_POST, handleChangeAddUserPassword);
    server.on("/download", HTTP_GET, handleFileDownload);
    server.on("/update", HTTP_GET, handleUpdatePage);
    server.on("/update", HTTP_POST, handleUpdate, handleUpdateUpload);
    server.on("/adduser", HTTP_POST, handleAddUser);
    server.on("/deleteuser", HTTP_GET, handleDeleteUser);
    server.onNotFound(handleNotFound);
    
    server.begin();
    Serial.println("Web server started.");
    syncUserListToSheets();
  }

  unsigned long disconnectedTimestamp = 0;
  unsigned long lastUploadTime = 0;

  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      if (!isConnected) {
        isConnected = true;
        Serial.println("WiFi connection restored!");
        lcd.setCursor(0,1);
        lcd.print(WiFi.localIP());
        delay(2000); 
        setupTime();
      }
      
      server.handleClient();
      ArduinoOTA.handle();
      if (millis() - lastUploadTime > UPLOAD_INTERVAL_MS) {
        sendDataToGoogleSheets();
        lastUploadTime = millis();
      }
    } else {
      if (isConnected) {
        isConnected = false;
        Serial.println("WiFi connection lost!");
        disconnectedTimestamp = millis();
        lastReconnectAttempt = disconnectedTimestamp;
      }

      if (millis() - lastReconnectAttempt > RECONNECT_INTERVAL) {
        Serial.println("Attempting to reconnect to WiFi...");
        WiFi.reconnect();
        lastReconnectAttempt = millis();
      }

      if (millis() - disconnectedTimestamp > RECOVERY_MODE_TIMEOUT && disconnectedTimestamp > 0) {
        Serial.println("Could not reconnect. Starting Recovery AP...");
        updateDisplayMessage("Recovery Mode", "Hotspot Active");
        
        WiFiManager recoveryManager;
        recoveryManager.setAPCallback([](WiFiManager*){
            Serial.println("Recovery AP 'RFID-System-Recovery' is now active.");
        });
        
        recoveryManager.setSaveConfigCallback(newConfigSavedCallback);
        recoveryManager.setConfigPortalTimeout(600);
        
        if (!recoveryManager.startConfigPortal("RFID-System-Recovery")) {
            Serial.println("Recovery portal timed out.");
        } else {
            Serial.println("New settings received from portal.");
        }

        Serial.println("Recovery process finished. Rebooting device...");
        delay(3000);
        ESP.restart();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void Task_RFID(void *pvParameters) {
  Serial.println("RFID Task started on Core 0.");
  
  struct tm timeinfo;
  getLocalTime(&timeinfo, 10000);
  if(timeinfo.tm_year > (2016 - 1900)) { lastDay = timeinfo.tm_yday; }
  lastCardActivityTime = millis();
  
  for (;;) {
    checkForDailyReset();
    
    if (millis() - lastCardActivityTime > MESSAGE_DISPLAY_MS && currentDisplayState == SHOWING_MESSAGE) {
      currentDisplayState = SHOWING_TIME;
    }
    
    if (currentDisplayState == SHOWING_TIME) {
       if (!isConnected) {
          updateDisplayMessage("WiFi Disconnected", "RFID Active...");
       } else {
          updateDisplayDateTime();
       }
    }
    
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      lastCardActivityTime = millis();
      currentDisplayState = SHOWING_MESSAGE;
      String uid = getUIDString(rfid.uid);
      if (lastScanTime.count(uid) && (millis() - lastScanTime[uid] < CARD_COOLDOWN_SECONDS * 1000)) {
        rfid.PICC_HaltA();
        vTaskDelay(pdMS_TO_TICKS(50));
        continue;
      }
      String name = getUserName(uid);
      time_t now = time(nullptr);
      String fullTimestamp = getFormattedTime(now);
      lastEventTime = fullTimestamp;
      lastEventUID = uid;
      lastEventName = name;
      lastEventTimer = millis();
      if (name == "Unknown User") {
        logInvalidAttemptToSd(uid);
        playBuzzer(2);
        lastEventAction = "INVALID";
        updateDisplayMessage("Access Denied", "");
      } else {
        bool isCurrentlyIn = userStatus[uid];
        if (!isCurrentlyIn) {
          logActivityToSd("ENTER", uid, name, 0, now);
          playBuzzer(1);
          userStatus[uid] = true;
          entryTime[uid] = now;
          lastEventAction = "ENTER";
          updateDisplayMessage("Welcome", name);
        } else {
          unsigned long duration = 0;
          time_t entry_time_val = 0;
          if(entryTime.count(uid)){
            duration = now - entryTime[uid];
            entry_time_val = entryTime[uid];
            entryTime.erase(uid);
          }
          logActivityToSd("EXIT", uid, name, duration, entry_time_val);
          playBuzzer(0);
          userStatus[uid] = false;
          lastEventAction = "EXIT";
          updateDisplayMessage("Goodbye", name);
        }
      }
      lastScanTime[uid] = millis();
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void newConfigSavedCallback() {
  Serial.println("New WiFi configuration saved via portal.");
  Serial.print("New saved SSID: ");
  Serial.println(wifiManager.getWiFiSSID());
  Serial.println("Rebooting system to apply new settings...");
  
  lcd.clear();
  lcd.print("New WiFi Saved!");
  lcd.setCursor(0, 1);
  lcd.print("Rebooting...");
  
  delay(4000);
  ESP.restart();
}

void setupInitialWifi() {
  wifiManager.setSaveConfigCallback(newConfigSavedCallback);
  wifiManager.setConfigPortalTimeout(300);

  lcd.clear();
  lcd.print("Connecting WiFi");
  Serial.println("Starting WiFiManager for initial setup...");

  if (!wifiManager.autoConnect("RFID-System-Setup")) {
    Serial.println("Initial connection failed. Recovery mode will be active.");
    lcd.clear();
    lcd.print("Setup Failed");
  } else {
    Serial.println("\nWiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    lcd.clear();
    lcd.print("WiFi Connected!");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
    delay(2000);
  }
}

void readStringFromEEPROM(int addrOffset, char* buffer, int bufSize) {
  int i;
  for (i = 0; i < bufSize; i++) {
    buffer[i] = EEPROM.read(addrOffset + i);
    if (buffer[i] == '\0') {
      break;
    }
  }
  buffer[i] = '\0';
}

void writeStringToEEPROM(int addrOffset, const String& str) {
  for (int i = 0; i < str.length(); i++) {
    EEPROM.write(addrOffset + i, str[i]);
  }
  EEPROM.write(addrOffset + str.length(), '\0');
  EEPROM.commit();
}

void loadPasswordsFromEEPROM() {
  char buffer[33];
  readStringFromEEPROM(0, buffer, 32);
  if (strlen(buffer) > 0) ADMIN_PASS = String(buffer);
  else writeStringToEEPROM(0, ADMIN_PASS);
  readStringFromEEPROM(32, buffer, 32);
  if (strlen(buffer) > 0) ADD_USER_PASS = String(buffer);
  else writeStringToEEPROM(32, ADD_USER_PASS);
  Serial.println("Passwords loaded from EEPROM.");
}

void setupOTA() {
  ArduinoOTA.setHostname("rfid-system");
  ArduinoOTA.setPassword("admin_ota");
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) type = "sketch";
      else type = "filesystem";
      Serial.println("Start updating " + type);
      updateDisplayMessage("UPDATING...", "DO NOT POWER OFF");
    })
    .onEnd([]() {
      Serial.println("\nEnd");
      updateDisplayMessage("Update Done!", "Rebooting...");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      lcd.setCursor(0, 1);
      lcd.print("Progress: " + String((progress / (total / 100))) + "%");
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
  ArduinoOTA.begin();
}

void loadUsersFromSd() {
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  File file = SD.open(USER_DATABASE_FILE, FILE_READ);
  if (file) {
    userDatabase.clear();
    while (file.available()) {
      String line = file.readStringUntil('\n'); line.trim();
      if (line.length() > 0) {
        int commaIndex = line.indexOf(',');
        if (commaIndex != -1) userDatabase[line.substring(0, commaIndex)] = line.substring(commaIndex + 1);
      }
    }
    file.close();
  }
  xSemaphoreGive(sdMutex);
}

void playBuzzer(int status) {
  if (status == 1) {
    tone(BUZZER_PIN, BEEP_FREQUENCY, 150);
    delay(180);
    tone(BUZZER_PIN, BEEP_FREQUENCY, 400);
  }
  else if (status == 0) {
    tone(BUZZER_PIN, BEEP_FREQUENCY, 100);
    delay(120);
    tone(BUZZER_PIN, BEEP_FREQUENCY, 100);
    delay(120);
    tone(BUZZER_PIN, BEEP_FREQUENCY, 100);
  }
  else if (status == 2) {
    tone(BUZZER_PIN, BEEP_FREQUENCY, 1000);
  }
}

String getFormattedTime(time_t timestamp) {
  if (WiFi.status() != WL_CONNECTED) return "No Time Sync";
  struct tm timeinfo;
  localtime_r(&timestamp, &timeinfo);
  char timeString[20];
  strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(timeString);
}

String getUserName(String uid) {
  if (userDatabase.count(uid)) return userDatabase[uid];
  return "Unknown User";
}

String getUIDString(MFRC522::Uid uid) {
  String uidStr = "";
  for (byte i = 0; i < uid.size; i++) {
    if (uid.uidByte[i] < 0x10) uidStr += "0";
    uidStr += String(uid.uidByte[i], HEX);
    if (i < uid.size - 1) uidStr += " ";
  }
  uidStr.toUpperCase();
  return uidStr;
}

void logActivityToSd(String event, String uid, String name, unsigned long duration, time_t event_time) {
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  String current_timestamp_str = getFormattedTime(time(nullptr));
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char logFilePath[40];
    strftime(logFilePath, sizeof(logFilePath), "/logs/%Y/%m/%d.csv", &timeinfo);
    String dirPath = String(logFilePath).substring(0, String(logFilePath).lastIndexOf('/'));
    if(!SD.exists(dirPath)) {
        String root_path = LOGS_DIRECTORY;
        String year_path = root_path + "/" + String(timeinfo.tm_year + 1900);
        if(!SD.exists(root_path)) SD.mkdir(root_path);
        if(!SD.exists(year_path)) SD.mkdir(year_path);
        if(!SD.exists(dirPath)) SD.mkdir(dirPath);
    }
    String fullLogEntry = current_timestamp_str + "," + event + "," + uid + "," + name + "," + String(duration) + "," + formatDuration(duration);
    File localFile = SD.open(logFilePath, FILE_APPEND);
    if(localFile) {
        if(localFile.size() == 0) { localFile.println("Timestamp,Action,UID,Name,Duration_sec,Duration_Formatted"); }
        localFile.println(fullLogEntry);
        localFile.close();
    }
  }
  
  if(WiFi.status() == WL_CONNECTED) {
    File queueFile = SD.open(G_SHEETS_QUEUE_FILE, FILE_APPEND);
    if (queueFile) {
        String dataForSheet = "";
        if (event == "ENTER") {
        String entry_time_str = getFormattedTime(event_time);
        dataForSheet = uid + "," + name + "," + entry_time_str + ",";
        }
        else if (event == "EXIT") {
        String exit_time_str = getFormattedTime(time(nullptr));
        dataForSheet = uid + "," + name + ",," + exit_time_str;
        }
        if (dataForSheet != "") {
        queueFile.println(dataForSheet);
        }
        queueFile.close();
    }
  }
  xSemaphoreGive(sdMutex);
}

void logInvalidAttemptToSd(String uid) {
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  File file = SD.open(INVALID_LOGS_FILE, FILE_APPEND);
  if (file) {
    if (file.size() == 0) { file.println("Timestamp,UID"); }
    file.println(getFormattedTime(time(nullptr)) + "," + uid);
    file.close();
  }
  xSemaphoreGive(sdMutex);
}

String formatDuration(unsigned long totalSeconds) {
  if (totalSeconds == 0) return "-";
  long hours = totalSeconds / 3600;
  long minutes = (totalSeconds % 3600) / 60;
  long seconds = totalSeconds % 60;
  String formatted = "";
  if (hours > 0) formatted += String(hours) + "h ";
  if (minutes > 0) formatted += String(minutes) + "m ";
  formatted += String(seconds) + "s";
  return formatted;
}

void checkForDailyReset() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;
  if (lastDay != -1 && lastDay != timeinfo.tm_yday) {
    userStatus.clear();
    entryTime.clear();
    lastDay = timeinfo.tm_yday;
    Serial.println("Daily reset of user status complete.");
  }
}

void sendDataToGoogleSheets() {
  if (WiFi.status() != WL_CONNECTED) return;
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  if (!SD.exists(G_SHEETS_QUEUE_FILE)) {
    xSemaphoreGive(sdMutex);
    return;
  }
  File queueFile = SD.open(G_SHEETS_QUEUE_FILE, FILE_READ);
  if (!queueFile || queueFile.size() == 0) {
    if(queueFile) queueFile.close();
    SD.remove(G_SHEETS_QUEUE_FILE);
    xSemaphoreGive(sdMutex);
    return;
  }
  queueFile.close();
  if (SD.exists(G_SHEETS_SENDING_FILE)) {
    SD.remove(G_SHEETS_SENDING_FILE);
  }
  bool renamed = SD.rename(G_SHEETS_QUEUE_FILE, G_SHEETS_SENDING_FILE);
  xSemaphoreGive(sdMutex);
  if (!renamed) {
    Serial.println("Failed to rename queue file for upload.");
    return;
  }
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  File sendingFile = SD.open(G_SHEETS_SENDING_FILE, FILE_READ);
  xSemaphoreGive(sdMutex);
  String payload = "";
  if (sendingFile) {
    while(sendingFile.available()){
      payload += sendingFile.readStringUntil('\n') + "\n";
    }
    sendingFile.close();
    payload.trim();
  } else {
    Serial.println("Failed to open sending file.");
    return;
  }
  if(payload.length() > 0) {
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();
    if (http.begin(client, GOOGLE_SCRIPT_ID)) {
      http.setTimeout(15000);
      http.addHeader("Content-Type", "text/plain");
      int httpCode = http.POST(payload);
      if(httpCode > 0) {
         Serial.printf("Google Sheets upload successful, code: %d\n", httpCode);
      } else {
         Serial.printf("Google Sheets upload failed, error: %s\n", http.errorToString(httpCode).c_str());
      }
      http.end();
    }
  }
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  if(SD.exists(G_SHEETS_SENDING_FILE)) {
    SD.remove(G_SHEETS_SENDING_FILE);
  }
  xSemaphoreGive(sdMutex);
}

void setupTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println("Time synchronized successfully.");
}

void updateDisplayMessage(String line1, String line2) {
  lcd.clear();
  int padding1 = (LCD_COLS - line1.length()) / 2;
  lcd.setCursor(padding1, 0);
  lcd.print(line1);
  int padding2 = (LCD_COLS - line2.length()) / 2;
  lcd.setCursor(padding2, 1);
  lcd.print(line2);
}

void updateDisplayDateTime() {
  static int lastSecond = -1;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 100)) {
    return;
  }
  if (timeinfo.tm_sec != lastSecond) {
    lastSecond = timeinfo.tm_sec;
    char dateStr[11];
    char timeStr[9];
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &timeinfo);
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
    updateDisplayMessage(String(dateStr), String(timeStr));
  }
}

void syncUserListToSheets() {
  if (WiFi.status() != WL_CONNECTED) return;
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  File userFile = SD.open(USER_DATABASE_FILE, FILE_READ);
  if (!userFile) {
    xSemaphoreGive(sdMutex);
    return;
  }
  String payload = "USER_LIST_UPDATE\n";
  while(userFile.available()) {
    payload += userFile.readStringUntil('\n') + "\n";
  }
  userFile.close();
  xSemaphoreGive(sdMutex);
  payload.trim();
  if(payload.length() > strlen("USER_LIST_UPDATE\n")) {
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();
    if (http.begin(client, GOOGLE_SCRIPT_ID)) {
      http.setTimeout(10000);
      http.addHeader("Content-Type", "text/plain");
      http.POST(payload);
      http.end();
      Serial.println("User list synchronized with Google Sheets.");
    }
  }
}

#include "web_content.h"