#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <SD.h>
#include <MFRC522.h>
#include <map>
#include "time.h"

// --- Note Frequencies for Buzzer ---
#define NOTE_C4  262
#define NOTE_G4  392
#define NOTE_B4  494
#define NOTE_D5  587
#define NOTE_F4  349

// --- SPI object for the second bus (HSPI) ---
SPIClass hspi(HSPI);

// =================================================================
// --- CONFIGURATION AREA ---
// =================================================================
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
const long  gmtOffset_sec = 10800;
const int   daylightOffset_sec = 0;

#define BUZZER_PIN 32
#define CARD_COOLDOWN_SECONDS 5
#define RST_PIN   22
#define SS_PIN    15
#define SD_CS_PIN       5 
#define HSPI_SCK_PIN    25
#define HSPI_MISO_PIN   26
#define HSPI_MOSI_PIN   27

#define USER_DATABASE_FILE "/users.csv"
#define LOGS_DIRECTORY "/logs"
#define TEMP_USER_FILE "/temp_users.csv"

// =================================================================
// --- Global Variables ---
// =================================================================
MFRC522 rfid(SS_PIN, RST_PIN);
WebServer server(80);
std::map<String, String> userDatabase;
std::map<String, bool> userStatus;
std::map<String, unsigned long> lastScanTime;
std::map<String, time_t> entryTime;
String lastEventUID = "N/A", lastEventName = "-", lastEventAction = "-", lastEventTime = "-";
int lastDay = -1;

// --- Function Prototypes ---
void loadUsersFromSd();
void playBuzzer(int status);
String getFormattedTime(time_t timestamp);
String getUserName(String uid);
String getUIDString(MFRC522::Uid uid);
void logActivityToSd(String event, String uid, String name, unsigned long duration);
String formatDuration(unsigned long totalSeconds);
void checkForDailyReset();

// Web Handler Prototypes
void handleRoot();
void handleCSS();
void handleData();
void handleAdmin();
void handleLogs();
void handleAddUser();
void handleDeleteUser();
void handleNotFound();

#include "web_content.h"

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(BUZZER_PIN, OUTPUT);
  Serial.println("\n-- RFID Access System - Stable Version --");

  // Initialize hardware first
  Serial.println("Initializing SPI buses...");
  SPI.begin(); 
  hspi.begin(HSPI_SCK_PIN, HSPI_MISO_PIN, HSPI_MOSI_PIN);
  
  Serial.println("Initializing SD Card...");
  if (!SD.begin(SD_CS_PIN, hspi)) {
    Serial.println("SD Card Mount Failed! Halting.");
    while(1);
  }
  
  Serial.println("Initializing RFID Reader...");
  rfid.PCD_Init();
  
  // Then connect to the network
  setupWifi();
  setupTime();

  loadUsersFromSd();
  
  struct tm timeinfo;
  if(getLocalTime(&timeinfo)){
    lastDay = timeinfo.tm_yday;
  }

  // Set up web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.on("/admin", HTTP_GET, handleAdmin);
  server.on("/logs", HTTP_GET, handleLogs);
  server.on("/style.css", HTTP_GET, handleCSS);
  server.on("/adduser", HTTP_POST, handleAddUser);
  server.on("/deleteuser", HTTP_GET, handleDeleteUser);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("Web server started. System is ready.");
  Serial.println("======================================");
}

// --- LOOP ---
void loop() {
  server.handleClient();
  checkForDailyReset();

  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String uid = getUIDString(rfid.uid);
    if (lastScanTime.count(uid) && (millis() - lastScanTime[uid] < CARD_COOLDOWN_SECONDS * 1000)) {
        rfid.PICC_HaltA(); return; 
    }
    
    String name = getUserName(uid);
    time_t now = time(nullptr);
    lastEventTime = getFormattedTime(now);
    lastEventUID = uid;
    lastEventName = name;
    
    if (name == "Unknown User") {
      logActivityToSd("INVALID", uid, name, 0);
      playBuzzer(2);
      lastEventAction = "INVALID";
    } else {
      bool isCurrentlyIn = userStatus[uid];
      if (!isCurrentlyIn) {
        logActivityToSd("ENTER", uid, name, 0);
        playBuzzer(1);
        userStatus[uid] = true;
        entryTime[uid] = now;
        lastEventAction = "ENTER";
      } else {
        unsigned long duration = 0;
        if(entryTime.count(uid)){
          duration = now - entryTime[uid];
          entryTime.erase(uid);
        }
        logActivityToSd("EXIT", uid, name, duration);
        playBuzzer(0);
        userStatus[uid] = false;
        lastEventAction = "EXIT";
      }
    }
    lastScanTime[uid] = millis();
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
}

// --- Helper Functions ---
void checkForDailyReset() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;
  if (lastDay != -1 && lastDay != timeinfo.tm_yday) {
    Serial.printf("New day detected. Resetting all user IN/OUT statuses.\n");
    userStatus.clear();
    entryTime.clear();
    lastDay = timeinfo.tm_yday;
  }
}

void logActivityToSd(String event, String uid, String name, unsigned long duration = 0) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) { Serial.println("Failed to get time for logging"); return; }
  
  char logFilePath[40];
  strftime(logFilePath, sizeof(logFilePath), "/logs/%Y/%m/%d.csv", &timeinfo);
  String path = String(logFilePath);
  String dirPath = path.substring(0, path.lastIndexOf('/'));

  if(!SD.exists(dirPath)) {
      String root_path = LOGS_DIRECTORY;
      String year_path = root_path + "/" + String(timeinfo.tm_year + 1900);
      if(!SD.exists(root_path)) SD.mkdir(root_path);
      if(!SD.exists(year_path)) SD.mkdir(year_path);
      if(!SD.exists(dirPath)) SD.mkdir(dirPath);
  }
  
  String logEntry = getFormattedTime(time(nullptr)) + "," + event + "," + uid + "," + name + "," + String(duration) + "," + formatDuration(duration);
  
  Serial.println(logEntry);
  File file = SD.open(logFilePath, FILE_APPEND);
  if (file) {
    if(file.size() == 0) { file.println("Time,Action,UID,Name,Duration(sec),Duration(Formatted)"); }
    file.println(logEntry);
    file.close();
  } else {
    Serial.println("Failed to open log file for writing.");
  }
}

void playBuzzer(int status) {
  if (status == 1) { tone(BUZZER_PIN, NOTE_B4, 150); delay(180); tone(BUZZER_PIN, NOTE_D5, 400); } 
  else if (status == 0) { tone(BUZZER_PIN, NOTE_G4, 150); delay(180); tone(BUZZER_PIN, NOTE_C4, 400); } 
  else if (status == 2) { tone(BUZZER_PIN, NOTE_F4, 200); delay(250); tone(BUZZER_PIN, NOTE_F4, 200); }
}

void loadUsersFromSd() {
  userDatabase.clear();
  File file = SD.open(USER_DATABASE_FILE);
  if (file) {
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