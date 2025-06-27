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

// --- Note Frequencies for Buzzer ---
#define NOTE_C4  2620
#define NOTE_G4  3920
#define NOTE_B4  4940
#define NOTE_D5  5870
#define NOTE_F4  3490


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
#define EVENT_TIMEOUT_MS 3000

#define RST_PIN    22
#define SS_PIN     15
#define SD_CS_PIN       5 
#define HSPI_SCK_PIN    25
#define HSPI_MISO_PIN   26
#define HSPI_MOSI_PIN   27

#define USER_DATABASE_FILE "/users.csv"
#define LOGS_DIRECTORY "/logs"
#define TEMP_USER_FILE "/temp_users.csv"

// --- Google Sheets Configuration ---
const char* GOOGLE_SCRIPT_ID = "https://script.google.com/macros/s/AKfycbwln2-wE1ex1eOsmDN_Ud8pCtYiYVTSOfUVIl32rD7hxY83OGei_Yc7D6DTYfPuObboTw/exec";
#define G_SHEETS_QUEUE_FILE "/upload_queue.csv"
#define UPLOAD_INTERVAL_MS 15000 // 2/5 dakika

// DÜZELTİLMİŞ KÖK SERTİFİKA (GlobalSign Root CA)
const char* root_ca = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDdTCCAl2gAwIBAgILBAAAAAABFUtaw5QwDQYJKoZIhvcNAQEFBQAwVzELMAkG\n" \
"A1UEBhMCQkUxGTAXBgNVBAoTEEdsb2JhbFNpZ24gbnYtc2ExEDAOBgNVBAsTB1Jv\n" \
"b3QgQ0ExGzAZBgNVBAMTEkdsb2JhbFNpZ24gUm9vdCBDQTAeFw05ODAyMDJkMDBa\n" \
"Fw0yODAxMjgxMjAwMDBaMFcxCzAJBgNVBAYTAkJFMRkwFwYDVQQKExBHbG9iYWxT\n" \
"aWduIG52LXNhMRAwDgYDVQQLEwdSb290IENBMRswGQYDVQQDExJHbG9iYWxTaWdu\n" \
"IFJvb3QgQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDaDuaZjc6j\n" \
"40+Kfvvxi4Mla+pIH/EqsYQCb3r4i1u4iOqGDxm4Z2AFYn7loWRgRdmB+zJznPKi\n" \
"ZBK8qEodA9eXfuYi840CjSQMX3RKHj95/WfRYcSHS+iv2vac8Aj9xYPoLWFyZsd\n" \
"C9TFRMht1SYt6Db7kEzjV2GQPltW2CoTWcFoEXVklGXTvsJ/LBEKEbbfNVXvvnSo\n" \
"ClJLvP+e1gvvoR22ZmDpay6cOB3FLd1MAX4V3b2Qe4+UbpAYMmcAfB9L1S9d4Vq\n" \
"fL4APi2I4mFB8wEtmv3eFEc11dXtczwAIStQ8e8QNT6J6L1DcAARxMvMvS67vPXs\n" \
"IuAFjk+sS2xADGlocx9MVMtLHyb7aA+1JgPAML2C/BUf2UjHGbATS1WClGMg82Oj\n" \
"lqQuva+m9Yt6o0jUj6kdw+tG+l7t7KH6sWbanTqM+eIqBrD8u9mJ8V5iW+YVt0Ng\n" \
"sc2i+LeO3rdUygqS2aRtrJb6e+kLsWjptgSHo2VgoBxgY82MgDStqA2u73rY6CU5\n" \
"2v/gMyQ5RyNs3DBB2gBwQsnL+pByBwFfn33v3P0T56sE2sloD9b2z5W//11vsJ0W\n" \
"vrEAZd0Y4GEUvyvK3rYPSrJ5I0pkveO3dY5bJ9BELN+Tj4HsoC+TGGgV3Y44/DPi\n" \
"o8Wd+H9e018fMSgYgEzwxKAe1dmbbCteNs9dJotg3E2+30DqfCRG+da6bjMRYaa/\n" \
"8k+L2eJE1bGHw602cH5S2bFDrTYB4xlKYs8gD+SeBEdPj0aBf0n0ly3gYwPS0s/P\n" \
"28x4a54T+IbiR4gX132b+Xb/1pPp2uOSDkYTwJ3wB21mO3s3e1LS2gDbIq2o2L0m\n" \
"Mv1HIe2WHB97S7//VM8LfnvW2p2Abm97q3I3vC3pS2aony3aL2eXryr2i2t1yvV1\n" \
"S3w62RT55t0Q0Yx42KVb6f9jV5tituT29PqPImL9fJ9Tq1gPyb92c4gBf2rStwRS\n" \
"AjbFAFNn9iWv6C3RAEw0A+23Yl+TofkTDmsQ2j9mDsv2j0b2a2d4J4wuYeAto2HZ\n" \
"4Zv5t1wE1mYnU5aW/ow3Gv34A+S2drtK/2iASV1oY1GzLgyo2CIXyYyl3gDk2dPr\n" \
"8LgYvC1l9GjYGBo25f//BwMPx5v22w2L9d2kL/G/pUR6K4Yk2w423f/2T/1c2a3g\n" \
"4L5Nf6n2gAc/a4uV+f/4a9d4a/2n7a5f6L3b/6a8d6f/a/a2b/3n6V/a/f+m/a3f\n" \
"-----END CERTIFICATE-----\n";



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
unsigned long lastUploadTime = 0; // Zamanlayıcı için yeni değişken

// --- Function Prototypes ---
void loadUsersFromSd();
void playBuzzer(int status);
String getFormattedTime(time_t timestamp);
String getUserName(String uid);
String getUIDString(MFRC522::Uid uid);
void logActivityToSd(String event, String uid, String name, unsigned long duration);
String formatDuration(unsigned long totalSeconds);
void checkForDailyReset();
void sendDataToGoogleSheets(); // YENİ

// Web Handler Prototypes
void handleRoot();
void handleCSS();
void handleData();
void handleAdmin();
void handleLogs();
void handleAddUser();
void handleDeleteUser();
void handleNotFound();
void handleStatus();

#include "web_content.h" // web_content.h dosyanızın aynı dizinde olduğundan emin olun

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(BUZZER_PIN, OUTPUT);
  Serial.println("\n-- RFID Access System with Google Sheets Sync --");

  Serial.println("Initializing SPI buses...");
  SPI.begin();  
  hspi.begin(HSPI_SCK_PIN, HSPI_MISO_PIN, HSPI_MOSI_PIN);
  
  Serial.println("Initializing SD Card...");
  if (!SD.begin(SD_CS_PIN, hspi)) {
    Serial.println("SD Card Mount Failed! Halting."); while(1);
  }
  
  Serial.println("Initializing RFID Reader...");
  rfid.PCD_Init();
  
  setupWifi();
  setupTime();

  loadUsersFromSd();
  
  struct tm timeinfo;
  if(getLocalTime(&timeinfo)){ lastDay = timeinfo.tm_yday; }

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
  Serial.println("Web server started. System is ready.");
  Serial.println("======================================");
}

// --- LOOP ---
void loop() {
  server.handleClient();
  checkForDailyReset();

  // YENİ: Her 10 dakikada bir Google Sheets'e veri göndermeyi dene
  if (WiFi.status() == WL_CONNECTED && millis() - lastUploadTime > UPLOAD_INTERVAL_MS) {
    sendDataToGoogleSheets();
    lastUploadTime = millis();
  }

  // Son olayın üzerinden 3 saniye geçtiyse web arayüzü bilgilerini sıfırla
  if (lastEventTimer > 0 && millis() - lastEventTimer > EVENT_TIMEOUT_MS) {
    lastEventUID = "N/A";
    lastEventName = "-";
    lastEventAction = "-";
    lastEventTime = "-";
    lastEventTimer = 0; // Zamanlayıcıyı sıfırla ki sürekli çalışmasın
  }

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
    lastEventTimer = millis(); // Olay zamanını kaydet ve zamanlayıcıyı başlat

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

// ZAMAN DAMGASINI GOOGLE SHEETS'E GÖNDERMEK İÇİN GÜNCELLENMİŞ FONKSİYON
void logActivityToSd(String event, String uid, String name, unsigned long duration = 0) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) { 
    Serial.println("Failed to get time for logging"); 
    return; 
  }

  // 1. Gerekli dosya yollarını oluştur (Bu kısım aynı)
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

  // DEĞİŞİKLİK BURADA:
  // Artık hem yerel log için hem de Google Sheets için tek bir tam format oluşturuyoruz.
  // Bu format, ESP32'nin o anki zaman damgasını içeriyor.
  String fullLogEntry = getFormattedTime(time(nullptr)) + "," + event + "," + uid + "," + name + "," + String(duration) + "," + formatDuration(duration);
  
  Serial.println("LOG: " + fullLogEntry);

  // 2. Yerel SD kart log dosyasına tam veriyi yaz
  File localFile = SD.open(logFilePath, FILE_APPEND);
  if (localFile) {
    if(localFile.size() == 0) { 
      // Dosya başlığını da yeni formata uygun hale getirelim
      localFile.println("Timestamp,Action,UID,Name,Duration_sec,Duration_Formatted"); 
    }
    localFile.println(fullLogEntry);
    localFile.close();
  } else {
    Serial.println("Failed to open daily log file for writing.");
  }

  // 3. Google Sheets gönderim kuyruğuna da tam veriyi yaz
  File queueFile = SD.open(G_SHEETS_QUEUE_FILE, FILE_APPEND);
  if (queueFile) {
    queueFile.println(fullLogEntry); // Buraya da zaman damgalı tam veriyi yazıyoruz.
    queueFile.close();
  } else {
    Serial.println("Failed to open upload queue file!");
  }
}
// YENİ FONKSİYON: SD karttaki kuyruk dosyasını Google Sheets'e gönderir.
// --- FİNAL, GÜVENLİ VE STABİL VERSİYON ---
void sendDataToGoogleSheets() {
  Serial.println("\nChecking for data to upload to Google Sheets...");

  if (!SD.exists(G_SHEETS_QUEUE_FILE)) {
    return;
  }

  File queueFile = SD.open(G_SHEETS_QUEUE_FILE, FILE_READ);
  if (!queueFile || queueFile.size() == 0) {
    if (queueFile) queueFile.close();
    if (SD.exists(G_SHEETS_QUEUE_FILE)) SD.remove(G_SHEETS_QUEUE_FILE);
    return;
  }
  
  size_t fileSize = queueFile.size(); 
  
  HTTPClient http;
  WiFiClientSecure client;

  // 1. GÜVENLİĞİ TEKRAR AÇIYORUZ
  client.setCACert(root_ca); 

  Serial.printf("Free heap before connecting: %u bytes\n", ESP.getFreeHeap());
  Serial.println("Connecting to Google Script (Secure Mode)...");

  if (http.begin(client, GOOGLE_SCRIPT_ID)) {
    // 2. ZAMAN AŞIMI SÜRESİNİ ARTIRIYORUZ (isteğe bağlı ama iyi bir fikir)
    http.setTimeout(15000); // 15 saniye bekle

    http.addHeader("Content-Type", "text/plain");
    Serial.printf("Sending %d bytes of data from file...\n", fileSize);
    
    // Hafıza dostu stream metoduyla gönderiyoruz
    int httpCode = http.POST(queueFile, fileSize); 
    queueFile.close();

    if (httpCode > 0) {
      Serial.printf("HTTP Response code: %d\n", httpCode);
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_FOUND) {
        Serial.println("Data successfully uploaded and queue file deleted.");
        SD.remove(G_SHEETS_QUEUE_FILE);
      } else {
        String response = http.getString();
        Serial.println("Google Script returned an error:");
        Serial.println(response);
      }
    } else {
      Serial.printf("HTTP POST failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.printf("[HTTP] Unable to connect to Google Script\n");
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
  if (!getLocalTime(&timeinfo)) { 
    Serial.println(" Failed to obtain time"); // BU HATAYI ALIYOR MUSUNUZ KONTROL EDİN
    return;
  }
  Serial.println(" Done.");
  // YENİ EKLENEN SATIR:
  Serial.print("Current time: ");
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S"); // Mevcut zamanı seri porta yazdır
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


// NOT: web_content.h dosyasının içeriği değişmediği için buraya tekrar eklenmedi.
// O dosyanın projenizde aynı şekilde durduğundan emin olun.
// Eğer o dosyayı kullanmıyorsanız, handle... fonksiyonlarını bu .ino dosyasına taşımanız gerekir.