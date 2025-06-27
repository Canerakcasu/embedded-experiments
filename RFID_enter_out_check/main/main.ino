// =================================================================
// --- KÜTÜPHANELER ---
// =================================================================
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

// --- FreeRTOS için Gerekli Tanımlamalar ---
TaskHandle_t Task_Network_Handle;
TaskHandle_t Task_RFID_Handle;
SemaphoreHandle_t sdMutex; // SD karta güvenli erişim için mutex

// --- Note Frequencies for Buzzer ---
#define NOTE_C4  262
#define NOTE_G4  392
#define NOTE_B4  494
#define NOTE_D5  587
#define NOTE_F4  349
#define NOTE_G5  784
#define NOTE_A5  880

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
const long  gmtOffset_sec = 7200; // Saat 1 saat ileri sorununu çözmek için güncellendi
const int   daylightOffset_sec = 0;

#define BUZZER_PIN 32
#define CARD_COOLDOWN_SECONDS 5
#define EVENT_TIMEOUT_MS 3000

#define RST_PIN    22
#define SS_PIN     15
#define SD_CS_PIN       5 
#define HSPI_SCK_PIN    25
#define HSPI_MISO_PIN   26
#define HSPI_MOSI_PIN   27

#define USER_DATABASE_FILE    "/users.csv"
#define LOGS_DIRECTORY        "/logs"
#define INVALID_LOGS_FILE     "/invalid_logs.csv"
#define G_SHEETS_QUEUE_FILE   "/upload_queue.csv"
#define G_SHEETS_SENDING_FILE "/upload_sending.csv"
#define TEMP_USER_FILE        "/temp_users.csv"

const char* GOOGLE_SCRIPT_ID = "https://script.google.com/macros/s/AKfycbyeeZ8cLBNxX2eUncqDnkBEQkIDmjH0HQbtCwC36OXyrBI7HK8wH1KUtSQjsiuAOpzVQg/exec";
#define UPLOAD_INTERVAL_MS 20000 // 10 dakika

// Google Sertifikası (root_ca) isteğiniz üzerine kaldırıldı.

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
unsigned long lastEventTimer = 0;

// =================================================================
// --- Function Prototypes ---
// =================================================================
void loadUsersFromSd();
void playBuzzer(int status);
String getFormattedTime(time_t timestamp);
String getUserName(String uid);
String getUIDString(MFRC522::Uid uid);
void logActivityToSd(String event, String uid, String name, unsigned long duration, time_t entry_time);
void logInvalidAttemptToSd(String uid);
String formatDuration(unsigned long totalSeconds);
void checkForDailyReset();
void sendDataToGoogleSheets();
void setupWifi();
void setupTime();

void Task_Network(void *pvParameters);
void Task_RFID(void *pvParameters);

// Web Handler Prototypes
#include "web_content.h"

// =================================================================
// --- SETUP ---
// =================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(BUZZER_PIN, OUTPUT);
  Serial.println("\n-- RFID Access System - Multicore (Insecure HTTPS) --");

  sdMutex = xSemaphoreCreateMutex();
  if (sdMutex == NULL) {
    Serial.println("Mutex can not be created. Halting.");
    while(1);
  }

  Serial.println("Initializing SPI buses...");
  SPI.begin();  
  hspi.begin(HSPI_SCK_PIN, HSPI_MISO_PIN, HSPI_MOSI_PIN);
  
  Serial.println("Initializing SD Card...");
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  if (!SD.begin(SD_CS_PIN, hspi)) {
    Serial.println("SD Card Mount Failed! Halting."); while(1);
  }
  xSemaphoreGive(sdMutex);

  Serial.println("Initializing RFID Reader...");
  rfid.PCD_Init();
  
  loadUsersFromSd();

  xTaskCreatePinnedToCore(Task_Network, "Network_Task", 10000, NULL, 1, &Task_Network_Handle, 1);
  xTaskCreatePinnedToCore(Task_RFID, "RFID_Task", 5000, NULL, 1, &Task_RFID_Handle, 0);

  Serial.println("Tasks created. System starting...");
}

// =================================================================
// --- CORE 1: Network ve Web Sunucusu Görevi ---
// =================================================================
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

// =================================================================
// --- CORE 0: RFID ve Ana Lojik Görevi ---
// =================================================================
void Task_RFID(void *pvParameters) {
  Serial.println("RFID Task started on Core 0.");

  struct tm timeinfo;
  if(getLocalTime(&timeinfo)){ lastDay = timeinfo.tm_yday; }

  for (;;) {
    checkForDailyReset();

    if (lastEventTimer > 0 && millis() - lastEventTimer > EVENT_TIMEOUT_MS) {
      lastEventUID = "N/A";
      lastEventName = "-";
      lastEventAction = "-";
      lastEventTime = "-";
      lastEventTimer = 0;
    }

    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      String uid = getUIDString(rfid.uid);
      if (lastScanTime.count(uid) && (millis() - lastScanTime[uid] < CARD_COOLDOWN_SECONDS * 1000)) {
          rfid.PICC_HaltA();
          vTaskDelay(50 / portTICK_PERIOD_MS);
          continue; 
      }
      
      String name = getUserName(uid);
      time_t now = time(nullptr);
      lastEventTime = getFormattedTime(now);
      lastEventUID = uid;
      lastEventName = name;
      lastEventTimer = millis();

      if (name == "Unknown User") {
        logInvalidAttemptToSd(uid);
        playBuzzer(2);
        lastEventAction = "INVALID";
      } else {
        bool isCurrentlyIn = userStatus[uid];
        if (!isCurrentlyIn) {
          logActivityToSd("ENTER", uid, name, 0, now);
          playBuzzer(1);
          userStatus[uid] = true;
          entryTime[uid] = now;
          lastEventAction = "ENTER";
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
        }
      }
      lastScanTime[uid] = millis();
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
    }

    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// =================================================================
// --- Ana Boş Döngü ---
// =================================================================
void loop() {
  vTaskDelete(NULL);
}

// =================================================================
// --- HELPER FUNCTIONS (Yardımcı Fonksiyonlar) ---
// =================================================================

void sendDataToGoogleSheets() {
  Serial.println("\nChecking for data to upload...");
  xSemaphoreTake(sdMutex, portMAX_DELAY);

  if (!SD.exists(G_SHEETS_QUEUE_FILE)) {
    xSemaphoreGive(sdMutex);
    return;
  }
  
  File queueFile = SD.open(G_SHEETS_QUEUE_FILE);
  if (queueFile.size() == 0) {
    queueFile.close();
    SD.remove(G_SHEETS_QUEUE_FILE);
    xSemaphoreGive(sdMutex);
    return;
  }
  queueFile.close();

  Serial.println("Renaming queue file to start upload process...");
  bool renamed = SD.rename(G_SHEETS_QUEUE_FILE, G_SHEETS_SENDING_FILE);
  xSemaphoreGive(sdMutex);

  if (renamed) {
    xSemaphoreTake(sdMutex, portMAX_DELAY);
    File sendingFile = SD.open(G_SHEETS_SENDING_FILE, FILE_READ);
    xSemaphoreGive(sdMutex);

    if (sendingFile) {
      String payload = "";
      while(sendingFile.available()){ payload += sendingFile.readStringUntil('\n') + "\n"; }
      sendingFile.close();
      payload.trim();

      if(payload.length() > 0) {
        HTTPClient http;
        WiFiClientSecure client;
        
        // --- İSTEĞİNİZ ÜZERİNE GÜVENLİK KONTROLÜ ATLANDI ---
        client.setInsecure();

        Serial.println("Connecting to Google Script (INSECURE MODE)...");
        if (http.begin(client, GOOGLE_SCRIPT_ID)) {
          http.setTimeout(15000);
          http.addHeader("Content-Type", "text/plain");
          int httpCode = http.POST(payload);
          http.end();
          Serial.printf("Upload attempt finished with HTTP code: %d\n", httpCode);
        }
      }
    }
  } 

  xSemaphoreTake(sdMutex, portMAX_DELAY);
  if(SD.exists(G_SHEETS_SENDING_FILE)) {
    Serial.println("Upload process finished. Deleting sending file.");
    SD.remove(G_SHEETS_SENDING_FILE);
  }
  xSemaphoreGive(sdMutex);
}

void logInvalidAttemptToSd(String uid) {
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  File file = SD.open(INVALID_LOGS_FILE, FILE_APPEND);
  if (file) {
    if (file.size() == 0) {
      file.println("Timestamp,UID");
    }
    file.println(getFormattedTime(time(nullptr)) + "," + uid);
    file.close();
  }
  xSemaphoreGive(sdMutex);
}

void logActivityToSd(String event, String uid, String name, unsigned long duration, time_t entry_time_val) {
  xSemaphoreTake(sdMutex, portMAX_DELAY);

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char logFilePath[40];
    strftime(logFilePath, sizeof(logFilePath), "/logs/%Y/%m/%d.csv", &timeinfo);
    
    String dirPath = String(logFilePath);
    dirPath = dirPath.substring(0, dirPath.lastIndexOf('/'));
    if(!SD.exists(dirPath)) {
        String root_path = LOGS_DIRECTORY;
        String year_path = root_path + "/" + String(timeinfo.tm_year + 1900);
        if(!SD.exists(root_path)) SD.mkdir(root_path);
        if(!SD.exists(year_path)) SD.mkdir(year_path);
        if(!SD.exists(dirPath)) SD.mkdir(dirPath);
    }

    String fullLogEntry = getFormattedTime(time(nullptr)) + "," + event + "," + uid + "," + name + "," + String(duration) + "," + formatDuration(duration);
    Serial.println("LOG: " + fullLogEntry);
    File localFile = SD.open(logFilePath, FILE_APPEND);
    if(localFile) {
        if(localFile.size() == 0) { localFile.println("Timestamp,Action,UID,Name,Duration_sec,Duration_Formatted"); }
        localFile.println(fullLogEntry);
        localFile.close();
    }
  }

  if (event == "EXIT") {
    String entry_time_str = getFormattedTime(entry_time_val);
    String exit_time_str = getFormattedTime(time(nullptr));
    String duration_str = formatDuration(duration);
    String sessionData = uid + "," + name + "," + entry_time_str + "," + exit_time_str + "," + duration_str;
    
    File queueFile = SD.open(G_SHEETS_QUEUE_FILE, FILE_APPEND);
    if (queueFile) {
      queueFile.println(sessionData);
      queueFile.close();
      Serial.println("SESSION_QUEUED: " + sessionData);
    }
  }

  xSemaphoreGive(sdMutex);
}

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

void playBuzzer(int status) {
  if (status == 1) { 
    tone(BUZZER_PIN, NOTE_B4, 150);
    delay(180);
    tone(BUZZER_PIN, NOTE_D5, 400);
  } else if (status == 0) { 
    tone(BUZZER_PIN, NOTE_G4, 150); 
    delay(180);
    tone(BUZZER_PIN, NOTE_C4, 400);
  } else if (status == 2) { 
    for (int i = 0; i < 5; i++) { 
      tone(BUZZER_PIN, NOTE_A5, 100); 
      delay(120);
      tone(BUZZER_PIN, NOTE_G5, 100); 
      delay(120);
    }
  }
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
  if (!getLocalTime(&timeinfo)) { 
    Serial.println(" Failed to obtain time");
    return; 
  }
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