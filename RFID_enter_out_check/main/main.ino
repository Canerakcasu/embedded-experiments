#include <SPI.h>
#include <SD.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <map>

// --- Nota Frekansları ---
// Bu blok, müzik notalarını frekans değerlerine çevirir.
#define NOTE_C4  140
#define NOTE_E4  700
#define NOTE_G4  400
#define NOTE_C5  1200
#define NOTE_G3  2600

// --- İkinci SPI hattı için nesne ---
SPIClass hspi(HSPI);

// --- WiFi Configuration ---
const char* ssid = "Ents_Test";     // Kendi WiFi ağınızın adını buraya girin
const char* password = "12345678"; // Kendi WiFi şifrenizi buraya girin

// --- Time Configuration (NTP) ---
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 10800; // Türkiye için GMT+3 (3600 * 3)
const int   daylightOffset_sec = 0;   // Yaz saati uygulaması yoksa 0

// --- Pin Tanımlamaları ---
#define BUZZER_PIN 32                   // Buzzer'ın bağlı olduğu pin
#define CARD_COOLDOWN_SECONDS 5         // Kartın tekrar okunabilmesi için geçmesi gereken süre (saniye)

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
std::map<String, String> userDatabase;        // UID -> İsim
std::map<String, bool> userStatus;            // UID -> Durum (true: İçeride, false: Dışarıda)
std::map<String, unsigned long> lastScanTime; // UID -> Son Okunma Zamanı (cooldown için)


void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT); // Buzzer pinini çıkış olarak ayarla

  Serial.println("\n-- Tek Okuyuculu Akıllı Geçiş Sistemi --");
  
  SPI.begin(); 
  rfid.PCD_Init();
  Serial.println("RFID reader initialized.");

  hspi.begin(HSPI_SCK_PIN, HSPI_MISO_PIN, HSPI_MOSI_PIN, SD_CS_PIN);
  
  Serial.print("Initializing SD card...");
  if (!SD.begin(SD_CS_PIN, hspi)) {
    Serial.println(" Card Mount Failed! Check your wiring, SD card formatting (FAT32) or the card itself.");
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
  // Yeni bir kart var mı ve okunabildi mi?
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String uid = getUIDString(rfid.uid);
    
    // Cooldown kontrolü: Aynı kart çok sık mı okunuyor?
    if (lastScanTime.count(uid) && (millis() - lastScanTime[uid] < CARD_COOLDOWN_SECONDS * 1000)) {
        rfid.PICC_HaltA();
        return; 
    }
    
    // Kullanıcıyı veritabanından bul
    String name = getUserName(uid);
    
    if (name == "Unknown User") {
      // --- KULLANICI TANINMIYOR ---
      logActivityToSd("INVALID", uid, name);
      playBuzzer(2); // Hata melodisi çal
    } else {
      // --- KULLANICI TANINIYOR ---
      bool isCurrentlyIn = userStatus[uid];

      if (!isCurrentlyIn) {
        // ---- KULLANICI GİRİŞ YAPIYOR (ENTER) ----
        logActivityToSd("ENTER", uid, name);
        playBuzzer(1); // Giriş melodisi çal
        userStatus[uid] = true; // Durumu "içeride" olarak güncelle
      } else {
        // ---- KULLANICI ÇIKIŞ YAPIYOR (EXIT) ----
        logActivityToSd("EXIT", uid, name);
        playBuzzer(0); // Çıkış melodisi çal
        userStatus[uid] = false; // Durumu "dışarıda" olarak güncelle
      }
    }

    // Bu kartın son okunma zamanını kaydet
    lastScanTime[uid] = millis();
    
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
}

// isEnter yerine durum parametresi (0:Çıkış, 1:Giriş, 2:Hata)
void playBuzzer(int status) {
  int noteDuration = 200; 
  int pauseBetweenNotes = 50; 

  if (status == 1) { // Giriş durumu
    // Yükselen C-major arpej (Başarı tonu)
    tone(BUZZER_PIN, NOTE_C4, noteDuration);
    delay(noteDuration + pauseBetweenNotes);
    tone(BUZZER_PIN, NOTE_E4, noteDuration);
    delay(noteDuration + pauseBetweenNotes);
    tone(BUZZER_PIN, NOTE_G4, noteDuration);
    delay(noteDuration + pauseBetweenNotes);
    tone(BUZZER_PIN, NOTE_C5, noteDuration * 2); // Son nota daha uzun
  } else if (status == 0) { // Çıkış durumu
    // Alçalan C-major arpej (Bitiş tonu)
    tone(BUZZER_PIN, NOTE_C5, noteDuration);
    delay(noteDuration + pauseBetweenNotes);
    tone(BUZZER_PIN, NOTE_G4, noteDuration);
    delay(noteDuration + pauseBetweenNotes);
    tone(BUZZER_PIN, NOTE_C4, noteDuration * 2); // Son nota daha uzun
  } else if (status == 2) { // Hata durumu
    // Düşük ve yavaş "yanlış" tonu
    tone(BUZZER_PIN, NOTE_G3, 800); // 800ms boyunca düşük bir ton
  }
}

void loadUsersFromSd() {
  File file = SD.open(USER_DATABASE_FILE);
  if (!file) {
    Serial.println("Failed to open users.csv from SD card. Make sure the file exists on the SD card root.");
    return;
  }
  Serial.println("Loading users from SD card:");
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue; // Skip empty lines
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