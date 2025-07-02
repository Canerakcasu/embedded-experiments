// ---------------------------------------------------------------- //
//                  LIBRARIES and INCLUDES                          //
// ---------------------------------------------------------------- //
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
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <EEPROM.h>
#include <WiFiManager.h>

#include "web_content.h" 

// ---------------------------------------------------------------- //
//                  GLOBAL VARIABLES and CONSTANTS                  //
// ---------------------------------------------------------------- //

// --- Tasks and Synchronization ---
TaskHandle_t Task_Network_Handle;
TaskHandle_t Task_RFID_Handle;
SemaphoreHandle_t sdMutex;

// --- EEPROM Configuration ---
#define EEPROM_SIZE 128
#define SSID_ADDR 64
#define PASS_ADDR 90

// --- Security Settings ---
String ADMIN_USER = "admin";
String ADMIN_PASS = "1234";
const char* ADD_USER_AUTH_USER = "user";
String ADD_USER_PASS = "123";

// --- Hardware and Other Constants ---
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

// --- LCD Settings ---
const int LCD_I2C_ADDR = 0x23;
const int LCD_COLS = 16;
const int LCD_ROWS = 2;
const int I2C_SDA_PIN = 13;
const int I2C_SCL_PIN = 14;

// --- File Paths and URLs ---
#define USER_DATABASE_FILE    "/users.csv"
#define LOGS_DIRECTORY        "/logs"
#define INVALID_LOGS_FILE     "/invalid_logs.csv"
#define G_SHEETS_QUEUE_FILE   "/upload_queue.csv"
#define G_SHEETS_SENDING_FILE "/upload_sending.csv"
#define TEMP_USER_FILE        "/temp_users.csv"
const char* GOOGLE_SCRIPT_ID = "https://script.google.com/macros/s/AKfycby1vGdWeMxtKsf0mf4i98NEv_NmrrHkRSRZ5-IMXtgSmRvFoyPZToPoie28pv-td8SnkQ/exec";
#define UPLOAD_INTERVAL_MS 60000

// --- Object Declarations ---
LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);
MFRC522 rfid(SS_PIN, RST_PIN);
WebServer server(80);
WiFiManager wifiManager;
bool portalRunning = false;

// --- Data Maps and State Variables ---
enum DisplayState { SHOWING_MESSAGE, SHOWING_TIME };
DisplayState currentDisplayState = SHOWING_TIME;
unsigned long lastCardActivityTime = 0;
std::map<String, String> userDatabase;
std::map<String, bool> userStatus;
std::map<String, unsigned long> lastScanTime;
std::map<String, time_t> entryTime;
String lastEventUID = "N/A", lastEventName = "-", lastEventAction = "-", lastEventTime = "-";
int lastDay = -1;
unsigned long lastEventTimer = 0;

struct NetworkConfig {
  char ssid[26];
  char password[26];
};
NetworkConfig networkConfig;

// ---------------------------------------------------------------- //
//                  FUNCTION PROTOTYPES                             //
// ---------------------------------------------------------------- //
void loadPasswordsFromEEPROM();
void loadUsersFromSd();
void saveConfigCallback();
void setupTime();
void setupOTA();
void syncUserListToSheets();
String getUIDString(MFRC522::Uid uid);
String getUserName(String uid);
String getFormattedTime(time_t timestamp);
String formatDuration(unsigned long totalSeconds);
void updateDisplayMessage(String line1, String line2);
void updateDisplayDateTime();
void logActivityToSd(String event, String uid, String name, unsigned long duration, time_t event_time);
void logInvalidAttemptToSd(String uid);
void playBuzzer(int status);
void checkForDailyReset();
void handleRoot();
void handleCSS();
void handleData();
void handleAddUserPage();
void handleGetLastUID();
void handleReboot();
void handleChangePassword();
void handleChangeAddUserPassword();
void listFiles(String& html, const char* dirname, int levels);
void handleFileDownload();
void handleUpdate();
void handleUpdateUpload();
void handleAdmin();
void handleLogs();
void handleDeleteUser();
void handleAddUser();
void handleNotFound();
void handleStatus();
void loadConfigurationFromEEPROM();
void saveConfigurationToEEPROM();

// ---------------------------------------------------------------- //
//                  SETUP and LOOP                                  //
// ---------------------------------------------------------------- //
void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(BUZZER_PIN, OUTPUT);
  Serial.println("\n-- RFID Access System Final v6.1 --");
  Serial.println("System is booting up...");

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("System Booting..");
  lcd.setCursor(0, 1);
  lcd.print("Please wait...");

  EEPROM.begin(EEPROM_SIZE);
  loadPasswordsFromEEPROM();

  sdMutex = xSemaphoreCreateMutex();
  
  // Önce SD kartın kullandığı HSPI yolunu başlat
  hspi.begin(HSPI_SCK_PIN, HSPI_MISO_PIN, HSPI_MOSI_PIN);
  
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  if (!SD.begin(SD_CS_PIN, hspi)) {
    Serial.println("SD Card Mount Failed! System halted.");
    updateDisplayMessage("SD Card Error", "System Halted");
    while (1);
  }
  xSemaphoreGive(sdMutex);
  Serial.println("SD card initialized successfully.");

  // *** EN ÖNEMLİ DÜZELTME: RFID için varsayılan SPI yolunu (VSPI) AÇIKÇA BAŞLAT ***
  // Bu, SD kartın kullandığı diğer SPI yolundan kaynaklanabilecek çakışmaları önler.
  SPI.begin();

  // Şimdi RFID okuyucuyu güvenle başlat
  rfid.PCD_Init();
  Serial.println("RFID reader initialized.");

  loadUsersFromSd();
  Serial.println("User database loaded from SD card.");

  xTaskCreatePinnedToCore(Task_Network, "Network_Task", 16000, NULL, 1, &Task_Network_Handle, 1);
  xTaskCreatePinnedToCore(Task_RFID, "RFID_Task", 8000, NULL, 1, &Task_RFID_Handle, 0);

  Serial.println("Core tasks have been created. System is running.");
}

void loop() {
  vTaskDelete(NULL);
}

// ---------------------------------------------------------------- //
//                  TASKS (FreeRTOS)                                //
// ---------------------------------------------------------------- //

void Task_Network(void* pvParameters) {
    Serial.println("Network Task started on Core 1.");
    loadConfigurationFromEEPROM();

    unsigned long lastReconnectAttempt = 0;
    const long reconnectInterval = 10000;
    bool server_ota_started = false;
    bool initial_attempt_done = false;

    wifiManager.setSaveConfigCallback(saveConfigCallback);
    wifiManager.setConfigPortalTimeout(300);

    for (;;) {
        if (WiFi.status() == WL_CONNECTED) {
            initial_attempt_done = true; 
            if (portalRunning) {
                wifiManager.stopConfigPortal();
                portalRunning = false;
                Serial.println("WiFi bağlandı, yapılandırma portalı kapatıldı.");
            }
            if (!server_ota_started) {
                Serial.println("Web sunucusu ve OTA başlatılıyor...");
                server.on("/", HTTP_GET, handleRoot);
                server.on("/data", HTTP_GET, handleData);
                server.on("/admin", HTTP_GET, handleAdmin);
                server.on("/logs", HTTP_GET, handleLogs);
                server.on("/status", HTTP_GET, handleStatus);
                server.on("/style.css", HTTP_GET, handleCSS);
                server.on("/adduserpage", HTTP_GET, handleAddUserPage);
                server.on("/getlastuid", HTTP_GET, handleGetLastUID);
                server.on("/reboot", HTTP_POST, handleReboot);
                server.on("/changepass", HTTP_POST, handleChangePassword);
                server.on("/changeadduserpass", HTTP_POST, handleChangeAddUserPassword);
                server.on("/download", HTTP_GET, handleFileDownload);
                server.on("/update", HTTP_GET, []() {
                    if (!server.authenticate(ADMIN_USER.c_str(), ADMIN_PASS.c_str())) { return server.requestAuthentication(); }
                    server.send_P(200, "text/html", PAGE_Update);
                });
                server.on("/update", HTTP_POST, handleUpdate, handleUpdateUpload);
                server.on("/adduser", HTTP_POST, handleAddUser);
                server.on("/deleteuser", HTTP_GET, handleDeleteUser);
                server.onNotFound(handleNotFound);
                server.begin();
                setupTime();
                setupOTA();
                syncUserListToSheets();
                server_ota_started = true;
            }
            server.handleClient();
            ArduinoOTA.handle();
        } else {
            server_ota_started = false;

            if (!initial_attempt_done && strlen(networkConfig.ssid) > 0) {
                 Serial.print("Sistem yeni başladı. Kayıtlı ağa bağlanmak için 30 saniye bekleniyor: ");
                 Serial.println(networkConfig.ssid);
                 WiFi.mode(WIFI_STA);
                 WiFi.begin(networkConfig.ssid, networkConfig.password);
                 unsigned long start_time = millis();
                 while(millis() - start_time < 30000) {
                    if(WiFi.status() == WL_CONNECTED) break;
                    vTaskDelay(pdMS_TO_TICKS(500));
                    Serial.print(".");
                 }
                 Serial.println();
                 initial_attempt_done = true;
                 continue;
            }

            if (!portalRunning) {
                Serial.println("Yapılandırma portalı başlatılıyor: 'RFID-Sistemi-Kurulum'");
                WiFi.mode(WIFI_AP_STA);
                wifiManager.startConfigPortal("RFID-Sistemi-Kurulum"); 
                portalRunning = true;
                lastReconnectAttempt = millis();
            }
            
            wifiManager.process();

            if (millis() - lastReconnectAttempt > reconnectInterval) {
                Serial.println("Arka planda yeniden bağlanma deneniyor...");
                WiFi.begin(networkConfig.ssid, networkConfig.password);
                lastReconnectAttempt = millis();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void Task_RFID(void* pvParameters) {
  Serial.println("RFID & Display Task started on Core 0.");
  struct tm timeinfo;
  getLocalTime(&timeinfo, 10000);
  if (timeinfo.tm_year > (2016 - 1900)) {
    lastDay = timeinfo.tm_yday;
  }
  lastCardActivityTime = millis();
  for (;;) {
    if (millis() - lastCardActivityTime > MESSAGE_DISPLAY_MS) {
        if (WiFi.status() != WL_CONNECTED) {
            if (!portalRunning) {
                 updateDisplayMessage("Connecting WiFi..", networkConfig.ssid);
            } else {
                 updateDisplayMessage("WiFi Setup:", "192.168.4.1");
            }
        } else {
            updateDisplayDateTime();
        }
    }
    if (lastEventTimer > 0 && millis() - lastEventTimer > EVENT_TIMEOUT_MS) {
      lastEventUID = "N/A"; lastEventName = "-"; lastEventAction = "-"; lastEventTime = "-";
      lastEventTimer = 0;
    }
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      lastCardActivityTime = millis();
      String uid = getUIDString(rfid.uid);
      if (lastScanTime.count(uid) && (millis() - lastScanTime[uid] < CARD_COOLDOWN_SECONDS * 1000)) {
        rfid.PICC_HaltA(); vTaskDelay(pdMS_TO_TICKS(50)); continue;
      }
      String name = getUserName(uid);
      time_t now = time(nullptr);
      lastEventTime = getFormattedTime(now);
      lastEventUID = uid; lastEventName = name; lastEventTimer = millis();
      if (name == "Unknown User") {
        logInvalidAttemptToSd(uid); playBuzzer(2); lastEventAction = "INVALID";
        updateDisplayMessage("Access Denied", "");
      } else {
        bool isCurrentlyIn = userStatus[uid];
        if (!isCurrentlyIn) {
          logActivityToSd("ENTER", uid, name, 0, now); playBuzzer(1); userStatus[uid] = true;
          entryTime[uid] = now; lastEventAction = "ENTER";
          updateDisplayMessage("Welcome", name);
        } else {
          unsigned long duration = 0; time_t entry_time_val = 0;
          if (entryTime.count(uid)) {
            duration = now - entryTime[uid]; entry_time_val = entryTime[uid]; entryTime.erase(uid);
          }
          logActivityToSd("EXIT", uid, name, duration, entry_time_val); playBuzzer(0);
          userStatus[uid] = false; lastEventAction = "EXIT";
          updateDisplayMessage("Goodbye", name);
        }
      }
      lastScanTime[uid] = millis(); rfid.PICC_HaltA(); rfid.PCD_StopCrypto1();
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ---------------------------------------------------------------- //
//                  HELPER and CALLBACK FUNCTIONS                   //
// ---------------------------------------------------------------- //

void readStringFromEEPROM(int addrOffset, char* buffer, int bufSize) {
  byte len = EEPROM.read(addrOffset);
  if (len > bufSize - 1) len = bufSize - 1;
  for (int i = 0; i < len; i++) {
    buffer[i] = EEPROM.read(addrOffset + 1 + i);
  }
  buffer[len] = '\0';
}

void writeStringToEEPROM(int addrOffset, const String& str) {
  byte len = str.length();
  if (len > 255) len = 255;
  EEPROM.write(addrOffset, len);
  for (int i = 0; i < len; i++) {
    EEPROM.write(addrOffset + 1 + i, str[i]);
  }
}

void loadConfigurationFromEEPROM() {
  readStringFromEEPROM(SSID_ADDR, networkConfig.ssid, 25);
  readStringFromEEPROM(PASS_ADDR, networkConfig.password, 25);
  Serial.println("Ağ ayarları EEPROM'dan yüklendi.");
}

void saveConfigurationToEEPROM() {
  writeStringToEEPROM(SSID_ADDR, networkConfig.ssid);
  writeStringToEEPROM(PASS_ADDR, networkConfig.password);
  EEPROM.commit();
  Serial.println("Yeni ağ ayarları EEPROM'a kaydedildi.");
}

void saveConfigCallback() {
    Serial.println(">>> Ayarlar kaydedildi, bilgiler alınıyor ve EEPROM'a yazılıyor...");
    strlcpy(networkConfig.ssid, wifiManager.getWiFiSSID().c_str(), sizeof(networkConfig.ssid));
    strlcpy(networkConfig.password, wifiManager.getWiFiPass().c_str(), sizeof(networkConfig.password));
    saveConfigurationToEEPROM();
    lcd.clear();
    lcd.print("Ayarlar Kaydedildi");
    lcd.setCursor(0,1);
    lcd.print("Yeniden Baslatiliyor");
    delay(2500);
    ESP.restart();
}

void loadPasswordsFromEEPROM() {
  char buffer[33];
  readStringFromEEPROM(0, buffer, 32);
  if (strlen(buffer) > 0) ADMIN_PASS = String(buffer);
  else writeStringToEEPROM(0, ADMIN_PASS);
  readStringFromEEPROM(32, buffer, 32);
  if (strlen(buffer) > 0) ADD_USER_PASS = String(buffer);
  else writeStringToEEPROM(32, ADD_USER_PASS);
  Serial.println("Yönetici şifreleri EEPROM'dan yüklendi.");
}

void setupOTA() {
  ArduinoOTA.setHostname("rfid-system");
  ArduinoOTA.setPassword("admin_ota");
  ArduinoOTA
    .onStart([]() { Serial.println("OTA Start"); updateDisplayMessage("UPDATING...", "DO NOT POWER OFF"); })
    .onEnd([]() { Serial.println("\nEnd"); updateDisplayMessage("Update Done!", "Rebooting..."); })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      lcd.setCursor(0, 1);
      lcd.print("Progress: " + String((progress / (total / 100))) + "%");
    })
    .onError([](ota_error_t error) { Serial.printf("Error[%u]: ", error); updateDisplayMessage("Update ERROR!", "Check console."); });
  ArduinoOTA.begin();
}

void setupTime() { configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); }

void playBuzzer(int status) {
  if (status == 1) {
    tone(BUZZER_PIN, BEEP_FREQUENCY, 150); vTaskDelay(pdMS_TO_TICKS(180)); tone(BUZZER_PIN, BEEP_FREQUENCY, 400);
  } else if (status == 0) {
    tone(BUZZER_PIN, BEEP_FREQUENCY, 100); vTaskDelay(pdMS_TO_TICKS(120));
    tone(BUZZER_PIN, BEEP_FREQUENCY, 100); vTaskDelay(pdMS_TO_TICKS(120));
    tone(BUZZER_PIN, BEEP_FREQUENCY, 100);
  } else if (status == 2) {
    tone(BUZZER_PIN, BEEP_FREQUENCY, 1000); vTaskDelay(pdMS_TO_TICKS(1100)); tone(BUZZER_PIN, BEEP_FREQUENCY, 1000);
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
  }
  xSemaphoreGive(sdMutex);
}

void syncUserListToSheets() {
  if (WiFi.status() != WL_CONNECTED) return;
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  File userFile = SD.open(USER_DATABASE_FILE, FILE_READ);
  if (!userFile) { xSemaphoreGive(sdMutex); return; }
  String payload = "USER_LIST_UPDATE\n";
  while(userFile.available()) { payload += userFile.readStringUntil('\n') + "\n"; }
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
    }
  }
}

void logActivityToSd(String event, String uid, String name, unsigned long duration, time_t event_time) {
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  String current_timestamp_str = getFormattedTime(time(nullptr));
  struct tm timeinfo;
  if (WiFi.status() == WL_CONNECTED && getLocalTime(&timeinfo)) {
    char logFilePath[40];
    strftime(logFilePath, sizeof(logFilePath), "/logs/%Y/%m/%d.csv", &timeinfo);
    String dirPath = String(logFilePath).substring(0, String(logFilePath).lastIndexOf('/'));
    if (!SD.exists(dirPath)) {
      String root_path = LOGS_DIRECTORY;
      String year_path = root_path + "/" + String(timeinfo.tm_year + 1900);
      if (!SD.exists(root_path)) SD.mkdir(root_path);
      if (!SD.exists(year_path)) SD.mkdir(year_path);
      if (!SD.exists(dirPath)) SD.mkdir(dirPath);
    }
    String fullLogEntry = current_timestamp_str + "," + event + "," + uid + "," + name + "," + String(duration) + "," + formatDuration(duration);
    File localFile = SD.open(logFilePath, FILE_APPEND);
    if (localFile) {
      if (localFile.size() == 0) { localFile.println("Timestamp,Action,UID,Name,Duration_sec,Duration_Formatted"); }
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
    } else if (event == "EXIT") {
      String exit_time_str = getFormattedTime(time(nullptr));
      dataForSheet = uid + "," + name + ",," + exit_time_str;
    }
    if (dataForSheet != "") { queueFile.println(dataForSheet); }
    queueFile.close();
  }
  xSemaphoreGive(sdMutex);
}

void updateDisplayMessage(String line1, String line2) {
  lcd.clear();
  int padding1 = (LCD_COLS - line1.length()) / 2; lcd.setCursor(padding1, 0); lcd.print(line1);
  int padding2 = (LCD_COLS - line2.length()) / 2; lcd.setCursor(padding2, 1); lcd.print(line2);
}

void updateDisplayDateTime() {
  static int lastSecond = -1;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) { updateDisplayMessage("Error", "No Time Sync"); return; }
  if (timeinfo.tm_sec != lastSecond) {
    lastSecond = timeinfo.tm_sec;
    char dateStr[11]; char timeStr[9];
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
    userStatus.clear(); entryTime.clear(); lastDay = timeinfo.tm_yday;
  }
}

String getUIDString(MFRC522::Uid uid) {
  String uidStr = "";
  for (byte i = 0; i < uid.size; i++) {
    if (uid.uidByte[i] < 0x10) uidStr += "0";
    uidStr += String(uid.uidByte[i], HEX);
    if (i < uid.size - 1) uidStr += " ";
  }
  uidStr.toUpperCase(); return uidStr;
}

String getUserName(String uid) {
  if (userDatabase.count(uid)) return userDatabase[uid];
  return "Unknown User";
}

String getFormattedTime(time_t timestamp) {
  if (WiFi.status() != WL_CONNECTED) return "No Time Sync";
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

// ---------------------------------------------------------------- //
//                  WEB SERVER HANDLER FUNCTIONS                    //
// ---------------------------------------------------------------- //
void handleRoot() { server.send_P(200, "text/html", PAGE_Main); }
void handleCSS() { server.send_P(200, "text/css", PAGE_CSS); }

void handleData() {
  StaticJsonDocument<256> doc;
  doc["time"] = lastEventTime;
  doc["uid"] = lastEventUID;
  doc["name"] = lastEventName;
  doc["action"] = lastEventAction;
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleAddUserPage() {
  if (!server.authenticate(ADD_USER_AUTH_USER, ADD_USER_PASS.c_str())) { return server.requestAuthentication(); }
  server.send_P(200, "text/html", PAGE_AddUser);
}

void handleGetLastUID() { server.send(200, "text/plain", lastEventUID); }

void handleReboot() {
  if (!server.authenticate(ADMIN_USER.c_str(), ADMIN_PASS.c_str())) return;
  server.send(200, "text/plain", "Rebooting in 3 seconds...");
  delay(3000);
  ESP.restart();
}

void handleChangePassword() {
  if (!server.authenticate(ADMIN_USER.c_str(), ADMIN_PASS.c_str())) return;
  if (server.hasArg("newpass")) {
    String newPass = server.arg("newpass");
    newPass.trim();
    if (newPass.length() > 3 && newPass.length() < 32) {
      writeStringToEEPROM(0, newPass);
      ADMIN_PASS = newPass;
      server.send(200, "text/plain", "Admin password changed successfully.");
    } else {
      server.send(400, "text/plain", "Password must be between 4 and 31 characters.");
    }
  } else {
    server.send(400, "text/plain", "No new password provided.");
  }
}

void handleChangeAddUserPassword() {
  if (!server.authenticate(ADMIN_USER.c_str(), ADMIN_PASS.c_str())) return;
  if (server.hasArg("newpass")) {
    String newPass = server.arg("newpass");
    newPass.trim();
    if (newPass.length() > 2 && newPass.length() < 32) {
      writeStringToEEPROM(32, newPass);
      ADD_USER_PASS = newPass;
      server.send(200, "text/plain", "'Add User' password changed successfully.");
    } else {
      server.send(400, "text/plain", "Password must be between 3 and 31 characters.");
    }
  } else {
    server.send(400, "text/plain", "No new password provided.");
  }
}

void listFiles(String& html, const char* dirname, int levels) {
    File root = SD.open(dirname);
    if (!root || !root.isDirectory()) return;
    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            if (levels) listFiles(html, file.path(), levels - 1);
        } else {
            String filePath = String(file.path());
            html += "<tr><td>" + filePath + "</td><td>" + String(file.size()) + " B</td>";
            html += "<td><a href='/download?file=" + filePath + "' class='btn-download'>Download</a></td></tr>";
        }
        file = root.openNextFile();
    }
}

void handleFileDownload() {
    if (!server.authenticate(ADMIN_USER.c_str(), ADMIN_PASS.c_str())) return;
    if (server.hasArg("file")) {
        String path = server.arg("file");
        if (path.indexOf("..") != -1 || !path.startsWith("/")) {
            server.send(400, "text/plain", "Invalid file path.");
            return;
        }
        String filename;
        int lastSlash = path.lastIndexOf('/');
        if (lastSlash != -1) {
            filename = path.substring(lastSlash + 1);
        } else {
            filename = path;
        }
        xSemaphoreTake(sdMutex, portMAX_DELAY);
        File file = SD.open(path, FILE_READ);
        xSemaphoreGive(sdMutex);
        if (file) {
            server.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
            server.streamFile(file, "application/octet-stream");
            file.close();
        } else {
            server.send(404, "text/plain", "File not found.");
        }
    } else {
        server.send(400, "text/plain", "File parameter missing.");
    }
}

void handleUpdate() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "UPDATE FAIL" : "UPDATE OK. Rebooting...");
    ESP.restart();
}

void handleUpdateUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_END) {
        if (!Update.end(true)) Update.printError(Serial);
    }
}

void handleAdmin() {
  if (!server.authenticate(ADMIN_USER.c_str(), ADMIN_PASS.c_str())) { return server.requestAuthentication(); }
  String html = "<html><head><title>Admin Panel</title><meta charset='UTF-8'><link href='/style.css' rel='stylesheet' type='text/css'></head><body><div class='container'>";
  html += "<h1>Admin Panel</h1><a href='/' class='home-link'>&larr; Back to Dashboard</a>";
  
  html += "<h2>WiFi Status</h2>";
  html += "<div class='data-grid'>";
  if(WiFi.status() == WL_CONNECTED){
    html += "<span>Durum:</span><span class='status status-ok'>BAĞLI</span>";
    html += "<span>SSID:</span><span>" + WiFi.SSID() + "</span>";
    html += "<span>IP Adresi:</span><span>" + WiFi.localIP().toString() + "</span>";
    html += "<span>Sinyal Gücü:</span><span>" + String(WiFi.RSSI()) + " dBm</span>";
  } else {
    html += "<span>Durum:</span><span class='status status-bad'>BAĞLI DEĞİL</span>";
    html += "<span>SSID:</span><span>-</span>";
    html += "<span>IP Adresi:</span><span>-</span>";
    html += "<span>Sinyal Gücü:</span><span>-</span>";
  }
  html += "</div>";

  html += "<h2>Security Settings</h2>";
  html += "<h3>Admin Password</h3>";
  html += "<form action='/changepass' method='post'>New Admin Password: <input type='password' name='newpass' required><br><input type='submit' value='Change'></form>";
  html += "<h3>'Add User' Page Password (user: user)</h3>";
  html += "<form action='/changeadduserpass' method='post'>New 'Add User' Password: <input type='password' name='newpass' required><br><input type='submit' value='Change'></form>";
  
  html += "<h2>User Management</h2>";
  html += "<table><tr><th>UID</th><th>Name</th><th>Current Status</th><th>Action</th></tr>";
  for (auto const& [uid, name] : userDatabase) {
    html += "<tr><td>" + uid + "</td><td>" + name + "</td><td>" + (userStatus[uid] ? "<span class='status status-in'>INSIDE</span>" : "<span class='status status-out'>OUTSIDE</span>") + "</td>";
    html += "<td><a href='/deleteuser?uid=" + uid + "' class='btn-delete' onclick='return confirm(\"Are you sure?\");'>Delete</a></td></tr>";
  }
  html += "</table>";
  
  html += "<h2>Device Management</h2>";
  html += "<form action='/reboot' method='post' onsubmit='return confirm(\"Reboot the device?\");'><button class='btn-reboot'>Reboot Device</button></form>";
  
  html += "<h2>Firmware Update</h2>";
  html += "<p>Update firmware via OTA. Upload a .bin file.</p>";
  html += "<form action='/update' method='get'><button>Go to Update Page</button></form>";
  
  html += "<h2>SD Card File Manager</h2>";
  html += "<table><tr><th>File Path</th><th>Size</th><th>Action</th></tr>";
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  listFiles(html, "/", 0);
  listFiles(html, LOGS_DIRECTORY, 2);
  xSemaphoreGive(sdMutex);
  html += "</table>";

  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

void handleLogs() {
  if (!server.authenticate(ADMIN_USER.c_str(), ADMIN_PASS.c_str())) { return server.requestAuthentication(); }
  String html = "<html><head><title>Activity Logs</title><meta charset='UTF-8'><link href='/style.css' rel='stylesheet' type='text/css'></head><body><div class='container'>";
  html += "<h1>Activity Logs</h1><a href='/' class='home-link'>&larr; Back to Dashboard</a>";
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){ html += "<h3>Error: Could not get time.</h3>"; }
  else {
    char logFilePath[40];
    strftime(logFilePath, sizeof(logFilePath), "/logs/%Y/%m/%d.csv", &timeinfo);
    xSemaphoreTake(sdMutex, portMAX_DELAY);
    File file = SD.open(logFilePath);
    xSemaphoreGive(sdMutex);
    if(file && file.size() > 0){
      html += "<h3>Daily Summary</h3><table style='width:50%;'><tr><th>Name</th><th>Total Time Inside</th></tr>";
      std::map<String, unsigned long> dailyTotals;
      xSemaphoreTake(sdMutex, portMAX_DELAY);
      file.seek(0);
      if(file.available()) file.readStringUntil('\n');
      while(file.available()){
        String line = file.readStringUntil('\n'); line.trim();
        if(line.length() > 0 && line.indexOf("EXIT") != -1) {
          String name = ""; String duration_s = "0";
          int lastIdx = -1;
          for(int i=0; i<4; i++){ lastIdx = line.indexOf(',', lastIdx+1); }
          name = line.substring(line.lastIndexOf(',', lastIdx-1)+1, lastIdx);
          duration_s = line.substring(lastIdx+1, line.indexOf(',', lastIdx+1));
          dailyTotals[name] += duration_s.toInt();
        }
      }
      xSemaphoreGive(sdMutex);
      if(dailyTotals.empty()){ html += "<tr><td colspan='2'>No completed sessions for today.</td></tr>"; }
      else { for (auto const& [name, totalDuration] : dailyTotals) { html += "<tr><td>" + name + "</td><td>" + formatDuration(totalDuration) + "</td></tr>"; } }
      html += "</table>";
      html += "<h3>Detailed Log</h3><table><tr><th>Time</th><th>Action</th><th>UID</th><th>Name</th><th>Duration</th></tr>";
      xSemaphoreTake(sdMutex, portMAX_DELAY);
      file.seek(0);
      if(file.available()) file.readStringUntil('\n');
      while(file.available()){
        String line = file.readStringUntil('\n'); line.trim();
        if(line.length() > 0){
          String part[6]; int lastIndex = -1;
          for(int i=0; i<5; i++){
            int commaIndex = line.indexOf(',', lastIndex+1);
            if(commaIndex == -1) { part[i] = line.substring(lastIndex+1); break; }
            part[i] = line.substring(lastIndex+1, commaIndex); lastIndex = commaIndex;
          }
          if (lastIndex != -1 && lastIndex < (int)line.length() - 1) part[5] = line.substring(lastIndex + 1); else part[5] = "-";
          html += "<tr><td>" + part[0] + "</td><td>" + part[1] + "</td><td>" + part[2] + "</td><td>" + part[3] + "</td><td>" + part[5] + "</td></tr>";
        }
      }
      file.close();
      xSemaphoreGive(sdMutex);
    } else { html += "<h3>No log file for today.</h3>"; }
  }
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

void handleDeleteUser() {
  if (!server.authenticate(ADMIN_USER.c_str(), ADMIN_PASS.c_str())) return;
  if (server.hasArg("uid")) {
    String uidToDelete = server.arg("uid");
    xSemaphoreTake(sdMutex, portMAX_DELAY);
    userStatus.erase(uidToDelete);
    entryTime.erase(uidToDelete);
    File originalFile = SD.open(USER_DATABASE_FILE, FILE_READ);
    File tempFile = SD.open(TEMP_USER_FILE, FILE_WRITE);
    if (!originalFile || !tempFile) {
      xSemaphoreGive(sdMutex);
      server.send(500, "text/plain", "File error.");
      return;
    }
    while (originalFile.available()) {
      String line = originalFile.readStringUntil('\n'); line.trim();
      if (line.length() > 0 && !line.startsWith(uidToDelete + ",")) { tempFile.println(line); }
    }
    originalFile.close();
    tempFile.close();
    SD.remove(USER_DATABASE_FILE);
    SD.rename(TEMP_USER_FILE, USER_DATABASE_FILE);
    xSemaphoreGive(sdMutex);
    loadUsersFromSd();
    syncUserListToSheets();
  }
  server.sendHeader("Location", "/admin", true);
  server.send(302, "text/plain", "");
}

void handleAddUser() {
  if (!server.authenticate(ADD_USER_AUTH_USER, ADD_USER_PASS.c_str())) return;
  if (server.hasArg("uid") && server.hasArg("name")) {
    String uid = server.arg("uid"); String name = server.arg("name");
    uid.trim(); name.trim();
    xSemaphoreTake(sdMutex, portMAX_DELAY);
    File file = SD.open(USER_DATABASE_FILE, FILE_APPEND);
    if (file) {
      file.println(uid + "," + name);
      file.close();
      xSemaphoreGive(sdMutex);
      loadUsersFromSd();
      syncUserListToSheets();
    } else {
      xSemaphoreGive(sdMutex);
    }
  }
  server.sendHeader("Location", "/admin", true);
  server.send(302, "text/plain", "");
}

void handleNotFound() { server.send(404, "text/plain", "404: Not Found"); }
void handleStatus() {}