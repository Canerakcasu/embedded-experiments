#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <SD.h>
#include <MFRC522.h>
#include <map>

// --- NOTA FREKANSLARI ---
#define NOTE_C4  262
#define NOTE_G4  392
#define NOTE_B4  494
#define NOTE_D5  587
#define NOTE_F4  349

// --- İkinci SPI hattı için nesne ---
SPIClass hspi(HSPI);

// =================================================================
// --- KONFİGÜRASYON ALANI ---
// =================================================================
IPAddress staticIP(192, 168,20,20);
IPAddress gateway(192, 168, 100, 1);
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
#define LOG_FILE "/activity_log.txt"
#define TEMP_USER_FILE "/temp_users.csv"

// =================================================================
// --- Global Değişkenler ---
// =================================================================
MFRC522 rfid(SS_PIN, RST_PIN);
WebServer server(80);
std::map<String, String> userDatabase;
std::map<String, bool> userStatus;
std::map<String, unsigned long> lastScanTime;
String lastEventUID = "N/A";
String lastEventName = "-";
String lastEventAction = "-";
String lastEventTime = "-";

// --- Web Handler Fonksiyon Prototipleri ---
// Bu blok, derleyiciye web_content.h içindeki fonksiyonların varlığını önceden bildirir.
void handleRoot();
void handleData();
void handleAdmin();
void handleLogs();
void handleCSS();
void handleAddUser();
void handleDeleteUser();
void handleNotFound();


// --- SETUP ---
void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  Serial.println("\n-- RFID Access System with Web UI --");

  setupWifi();
  
  SPI.begin(); 
  rfid.PCD_Init();
  hspi.begin(HSPI_SCK_PIN, HSPI_MISO_PIN, HSPI_MOSI_PIN, SD_CS_PIN);
  if (!SD.begin(SD_CS_PIN, hspi)) {
    Serial.println("SD Card Mount Failed!"); while(1);
  }
  loadUsersFromSd();
  setupTime();

  // Web Sunucusu Rotalarını Ayarla
  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.on("/admin", HTTP_GET, handleAdmin);
  server.on("/logs", HTTP_GET, handleLogs);
  server.on("/style.css", HTTP_GET, handleCSS);
  server.on("/adduser", HTTP_POST, handleAddUser);
  server.on("/deleteuser", HTTP_GET, handleDeleteUser);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("Web server started. Ready for requests.");
  Serial.println("======================================");
}

// --- LOOP ---
void loop() {
  server.handleClient();

  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String uid = getUIDString(rfid.uid);
    if (lastScanTime.count(uid) && (millis() - lastScanTime[uid] < CARD_COOLDOWN_SECONDS * 1000)) {
        rfid.PICC_HaltA(); return; 
    }
    String name = getUserName(uid);
    lastEventTime = getFormattedTime();
    lastEventUID = uid;
    lastEventName = name;

    if (name == "Unknown User") {
      logActivityToSd("INVALID", uid, name);
      playBuzzer(2);
      lastEventAction = "INVALID";
    } else {
      bool isCurrentlyIn = userStatus[uid];
      if (!isCurrentlyIn) {
        logActivityToSd("ENTER", uid, name);
        playBuzzer(1);
        userStatus[uid] = true;
        lastEventAction = "ENTER";
      } else {
        logActivityToSd("EXIT", uid, name);
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

// --- Donanım ve Yardımcı Fonksiyonlar ---
void playBuzzer(int status) {
  if (status == 1) { // Giriş: Onaylandı
    tone(BUZZER_PIN, NOTE_B4, 150);
    delay(180);
    tone(BUZZER_PIN, NOTE_D5, 400);
  } else if (status == 0) { // Çıkış: Bay Bay
    tone(BUZZER_PIN, NOTE_G4, 150);
    delay(180);
    tone(BUZZER_PIN, NOTE_C4, 400);
  } else if (status == 2) { // Hata: Reddedildi
    tone(BUZZER_PIN, NOTE_F4, 200);
    delay(250);
    tone(BUZZER_PIN, NOTE_F4, 200);
  }
}
void loadUsersFromSd() {
  userDatabase.clear();
  File file = SD.open(USER_DATABASE_FILE);
  if (!file) { return; }
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;
    int commaIndex = line.indexOf(',');
    if (commaIndex != -1) {
      userDatabase[line.substring(0, commaIndex)] = line.substring(commaIndex + 1);
    }
  }
  file.close();
}
void logActivityToSd(String event, String uid, String name) {
  String logEntry = getFormattedTime() + "," + event + "," + uid + "," + name;
  Serial.println(logEntry);
  File file = SD.open(LOG_FILE, FILE_APPEND);
  if (file) { file.println(logEntry); file.close(); }
}
void setupWifi() {
  Serial.print("Configuring static IP address...");
  if (!WiFi.config(staticIP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("Static IP Failed to configure");
  }
  Serial.println(" Done.");
  
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500); Serial.print("."); attempts++;
  }
  if(WiFi.status() == WL_CONNECTED){
    Serial.println("\nWiFi Connected!");
    Serial.print("Access Point: http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nCould not connect to WiFi.");
  }
}
void setupTime() {
  Serial.print("Configuring time...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println(" Failed to obtain time"); return;
  }
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
String getFormattedTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "Time N/A";
  char timeString[20];
  strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(timeString);
}

// Web arayüzü kodlarını ve handler'larını içeren dosyayı çağır
#include "web_content.h"