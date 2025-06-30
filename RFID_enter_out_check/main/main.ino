#include <WiFi.h>
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

TaskHandle_t Task_Network_Handle;
TaskHandle_t Task_RFID_Handle;
SemaphoreHandle_t sdMutex;

#define NOTE_C4  2620
#define NOTE_G4  3920
#define NOTE_B4  4940
#define NOTE_D5  5870
#define NOTE_F4  3490
#define NOTE_G5  7840
#define NOTE_A5  8800

SPIClass hspi(HSPI);

IPAddress staticIP(192, 168, 20, 20);
IPAddress gateway(192, 168, 20, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

const char* ssid = "Ents_Test";
const char* password = "12345678";

const char* ADMIN_USER = "admin";
const char* ADMIN_PASS = "1234";

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7200;
const int   daylightOffset_sec = 0;

#define BUZZER_PIN 32
#define CARD_COOLDOWN_SECONDS 5
#define EVENT_TIMEOUT_MS 3000 
#define MESSAGE_DISPLAY_MS 5000 

#define RST_PIN    22
#define SS_PIN     15
#define SD_CS_PIN       5 
#define HSPI_SCK_PIN    25
#define HSPI_MISO_PIN   26
#define HSPI_MOSI_PIN   27

const int LCD_I2C_ADDR = 0x23;
const int LCD_COLS = 16;
const int LCD_ROWS = 2;
const int I2C_SDA_PIN = 13;
const int I2C_SCL_PIN = 14;
LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);
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
#define UPLOAD_INTERVAL_MS 120000 // 2mins

MFRC522 rfid(SS_PIN, RST_PIN);
WebServer server(80);
std::map<String, String> userDatabase;
std::map<String, bool> userStatus;
std::map<String, unsigned long> lastScanTime;
std::map<String, time_t> entryTime;
String lastEventUID = "N/A", lastEventName = "-", lastEventAction = "-", lastEventTime = "-";
int lastDay = -1;
unsigned long lastEventTimer = 0;

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
void setupWifi();
void setupTime();
void updateDisplayMessage(String line1, String line2);
void updateDisplayDateTime();
void syncUserListToSheets();

void Task_Network(void *pvParameters);
void Task_RFID(void *pvParameters);

#include "web_content.h"


void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(BUZZER_PIN, OUTPUT);
  Serial.println("\n-- RFID Access System - Final Stable Version --");

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN); 
  
  lcd.init();
  lcd.backlight();
  Serial.println("16x2 I2C LCD initialized.");
  lcd.setCursor(0,0);
  lcd.print("System Ready");
  delay(2000);
  
  sdMutex = xSemaphoreCreateMutex();
  if (sdMutex == NULL) { Serial.println("Mutex can not be created."); while(1); }

  SPI.begin();  
  hspi.begin(HSPI_SCK_PIN, HSPI_MISO_PIN, HSPI_MOSI_PIN);
  
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  if (!SD.begin(SD_CS_PIN, hspi)) { Serial.println("SD Card Mount Failed!"); while(1); }
  xSemaphoreGive(sdMutex);

  rfid.PCD_Init();
  loadUsersFromSd();

  xTaskCreatePinnedToCore(Task_Network, "Network_Task", 10000, NULL, 1, &Task_Network_Handle, 1);
  xTaskCreatePinnedToCore(Task_RFID, "RFID_Task", 5000, NULL, 1, &Task_RFID_Handle, 0);

  Serial.println("Tasks created. System starting...");
}


void Task_Network(void *pvParameters) {
  Serial.println("Network Task started on Core 1.");
  
  setupWifi();
  setupTime();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.on("/admin", HTTP_GET, handleAdmin);
  server.on("/logs", HTTP_GET, handleLogs);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/style.css", HTTP_GET, handleCSS);
  server.on("/adduser", HTTP_POST, handleAddUser);
  server.on("/deleteuser", HTTP_GET, handleDeleteUser);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Web server started.");

  syncUserListToSheets(); 

  unsigned long lastUploadTime = 0;

  for (;;) {
    server.handleClient();
    
    if (WiFi.status() == WL_CONNECTED && millis() - lastUploadTime > UPLOAD_INTERVAL_MS) {
      sendDataToGoogleSheets();
      lastUploadTime = millis();
    }
    
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}


void Task_RFID(void *pvParameters) {
  Serial.println("RFID & Display Task started on Core 0.");

  struct tm timeinfo;
  getLocalTime(&timeinfo, 10000);
  if(timeinfo.tm_year > (2016 - 1900)) {
    lastDay = timeinfo.tm_yday;
  }
  lastCardActivityTime = millis();

  for (;;) {
    checkForDailyReset();

    if (millis() - lastCardActivityTime > MESSAGE_DISPLAY_MS && currentDisplayState == SHOWING_MESSAGE) {
      currentDisplayState = SHOWING_TIME;
    }
    
    if (currentDisplayState == SHOWING_TIME) {
      updateDisplayDateTime();
    }
    
    if (lastEventTimer > 0 && millis() - lastEventTimer > EVENT_TIMEOUT_MS) {
      lastEventUID = "N/A";
      lastEventName = "-";
      lastEventAction = "-";
      lastEventTime = "-";
      lastEventTimer = 0;
    }

    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      lastCardActivityTime = millis();
      currentDisplayState = SHOWING_MESSAGE;

      String uid = getUIDString(rfid.uid);
      if (lastScanTime.count(uid) && (millis() - lastScanTime[uid] < CARD_COOLDOWN_SECONDS * 1000)) {
          rfid.PICC_HaltA();
          vTaskDelay(50 / portTICK_PERIOD_MS);
          continue; 
      }
      
      String name = getUserName(uid);
      time_t now = time(nullptr);
      String fullTimestamp = getFormattedTime(now);
      String eventTimeStr = fullTimestamp.substring(11);

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

    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}


void loop() {
  vTaskDelete(NULL);
}


void sendDataToGoogleSheets() {
  Serial.println("\nChecking for data to upload...");
  
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
    Serial.println("Warning: Stale sending file found. Deleting it.");
    SD.remove(G_SHEETS_SENDING_FILE);
  }
  
  bool renamed = SD.rename(G_SHEETS_QUEUE_FILE, G_SHEETS_SENDING_FILE);
  xSemaphoreGive(sdMutex);

  if (!renamed) {
    Serial.println("ERROR: Could not rename queue file. Aborting sync.");
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
    Serial.println("ERROR: Could not read sending file after renaming.");
    return; 
  }

  if(payload.length() > 0) {
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();

    if (http.begin(client, GOOGLE_SCRIPT_ID)) {
      http.setTimeout(15000);
      http.addHeader("Content-Type", "text/plain");
      Serial.println("Sending payload to Google Script...");
      int httpCode = http.POST(payload);
      http.end();
      Serial.printf("Upload attempt finished with HTTP code: %d\n", httpCode);
    } else {
      Serial.println("HTTP client failed to begin connection.");
    }
  }

  xSemaphoreTake(sdMutex, portMAX_DELAY);
  if(SD.exists(G_SHEETS_SENDING_FILE)) {
    SD.remove(G_SHEETS_SENDING_FILE);
  }
  xSemaphoreGive(sdMutex);
}

void syncUserListToSheets() {
  Serial.println("Syncing user list to Google Sheets...");
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  File userFile = SD.open(USER_DATABASE_FILE, FILE_READ);
  
  if (!userFile) {
    Serial.println("Failed to open users.csv for syncing.");
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
      int httpCode = http.POST(payload);
      http.end();
      Serial.printf("User list sync finished with HTTP code: %d\n", httpCode);
    }
  }
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
    Serial.println("LOG: " + fullLogEntry);
    File localFile = SD.open(logFilePath, FILE_APPEND);
    if(localFile) {
        if(localFile.size() == 0) { localFile.println("Timestamp,Action,UID,Name,Duration_sec,Duration_Formatted"); }
        localFile.println(fullLogEntry);
        localFile.close();
    }
  }

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
  xSemaphoreGive(sdMutex);
}

// Diğer tüm yardımcı fonksiyonlar...
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
  if (!getLocalTime(&timeinfo)) {
    updateDisplayMessage("Error", "No Time Sync");
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

void checkForDailyReset() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;
  if (lastDay != -1 && lastDay != timeinfo.tm_yday) {
    userStatus.clear();
    entryTime.clear();
    lastDay = timeinfo.tm_yday;
  }
}

void playBuzzer(int status) {
  if (status == 1) { tone(BUZZER_PIN, NOTE_B4, 150); delay(180); tone(BUZZER_PIN, NOTE_D5, 400); } 
  else if (status == 0) { tone(BUZZER_PIN, NOTE_G4, 150); delay(180); tone(BUZZER_PIN, NOTE_C4, 400); } 
  else if (status == 2) { for (int i = 0; i < 5; i++) { tone(BUZZER_PIN, NOTE_A5, 100); delay(120); tone(BUZZER_PIN, NOTE_G5, 100); delay(120); } }
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
    Serial.printf("%d users loaded from SD card.\n", userDatabase.size());
  } else {
    Serial.println("Could not find users.csv file.");
  }
  xSemaphoreGive(sdMutex);
}

void setupWifi() {
  Serial.print("Connecting to WiFi...");
  WiFi.config(staticIP, gateway, subnet, primaryDNS, secondaryDNS);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi Connected!");
  Serial.print("Access Point: http://"); Serial.println(WiFi.localIP());
}

void setupTime() {
  Serial.print("Configuring time...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) { Serial.println(" Failed to obtain time"); return; }
  Serial.println(" Done.");
  Serial.print("Current time: ");
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
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

String getUserName(String uid) {
  if (userDatabase.count(uid)) return userDatabase[uid];
  return "Unknown User";
}

String getFormattedTime(time_t timestamp) {
  struct tm timeinfo;
  localtime_r(&timestamp, &timeinfo);
  char timeString[20];
  strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(timeString);
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