#include <SPI.h>
#include <SD.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <map>

// --- Genişletilmiş Nota Frekansları ---
#define NOTE_C4  1700
#define NOTE_D4  2600
#define NOTE_E4  2600
#define NOTE_F4  5000
#define NOTE_G4  2600
#define NOTE_A4  2600
#define NOTE_B4  2600
#define NOTE_C5  2600
#define NOTE_D5  2600
#define NOTE_E5  2600
#define NOTE_F5  2600
#define NOTE_G5  2600
#define NOTE_A5  2600
#define NOTE_B5  2600
#define NOTE_G3  2600

// --- İkinci SPI hattı için nesne ---
SPIClass hspi(HSPI);

// --- WiFi Configuration ---
const char* ssid = "Ents_Test";
const char* password = "12345678";

// --- Time Configuration (NTP) ---
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 10800;
const int   daylightOffset_sec = 0;

// --- Pin Tanımlamaları ---
#define BUZZER_PIN 32
#define CARD_COOLDOWN_SECONDS 5

// --- RFID Reader (Tek Okuyucu) ---
#define RST_PIN   22
#define SS_PIN    15
MFRC522 rfid(SS_PIN, RST_PIN);

// --- SD Card (HSPI bus on SAFE PINS) ---
#define SD_CS_PIN       5 
#define HSPI_SCK_PIN    25
#define HSPI_MISO_PIN   26
#define HSPI_MOSI_PIN   27

// --- File Paths on SD Card ---
#define USER_DATABASE_FILE "/users.csv"
#define LOG_FILE "/activity_log.txt"

// --- Veritabanları ---
std::map<String, String> userDatabase;
std::map<String, bool> userStatus;
std::map<String, unsigned long> lastScanTime;


void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);

  Serial.println("\n-- Tek Okuyuculu Akıllı Geçiş Sistemi --");
  
  SPI.begin(); 
  rfid.PCD_Init();
  Serial.println("RFID reader initialized.");

  hspi.begin(HSPI_SCK_PIN, HSPI_MISO_PIN, HSPI_MOSI_PIN, SD_CS_PIN);
  
  Serial.print("Initializing SD card...");
  if (!SD.begin(SD_CS_PIN, hspi)) {
    Serial.println(" Card Mount Failed!");
    while (1);
  }
  Serial.println(" SD card initialized.");
  
  loadUsersFromSd();
  setupWifi();
  setupTime();

  Serial.println("\nSistem Hazir. Lutfen kartinizi okutun.");
  Serial.println("======================================");
}

void loop() {
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String uid = getUIDString(rfid.uid);
    
    if (lastScanTime.count(uid) && (millis() - lastScanTime[uid] < CARD_COOLDOWN_SECONDS * 1000)) {
        rfid.PICC_HaltA();
        return; 
    }
    
    String name = getUserName(uid);
    
    if (name == "Unknown User") {
      logActivityToSd("INVALID", uid, name);
      playBuzzer(2); // Hata melodisi çal
    } else {
      bool isCurrentlyIn = userStatus[uid];
      if (!isCurrentlyIn) {
        logActivityToSd("ENTER", uid, name);
        playBuzzer(1); // Giriş melodisi çal
        userStatus[uid] = true;
      } else {
        logActivityToSd("EXIT", uid, name);
        playBuzzer(0); // Çıkış melodisi çal
        userStatus[uid] = false;
      }
    }
    lastScanTime[uid] = millis();
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
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
  File file = SD.open(USER_DATABASE_FILE);
  if (!file) {
    Serial.println("Failed to open users.csv from SD card.");
    return;
  }
  Serial.println("Loading users from SD card:");
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;
    int commaIndex = line.indexOf(',');
    if (commaIndex != -1) {
      String uid = line.substring(0, commaIndex);
      String name = line.substring(commaIndex + 1);
      userDatabase[uid] = name;
      Serial.printf("  - Loaded User: %s -> %s\n", uid.c_str(), name.c_str());
    }
  }
  file.close();
}

void logActivityToSd(String event, String uid, String name) {
  String logEntry = getFormattedTime() + "," + event + "," + uid + "," + name;
  Serial.println(logEntry);
  File file = SD.open(LOG_FILE, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open log file for writing.");
    return;
  }
  file.println(logEntry);
  file.close();
}

void setupWifi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void setupTime() {
  Serial.print("Configuring time from NTP server...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println(" Failed to obtain time");
    return;
  }
  Serial.println(" Done.");
}

String getUIDString(MFRC522::Uid uid) {
  String uidStr = "";
  for (byte i = 0; i < uid.size; i++) {
    if (uid.uidByte[i] < 0x10) {
      uidStr += "0";
    }
    uidStr += String(uid.uidByte[i], HEX);
    if (i < uid.size - 1) {
      uidStr += " ";
    }
  }
  uidStr.toUpperCase();
  return uidStr;
}

String getUserName(String uid) {
  if (userDatabase.count(uid)) {
    return userDatabase[uid];
  }
  return "Unknown User";
}

String getFormattedTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Time not available";
  }
  char timeString[20];
  strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(timeString);
}