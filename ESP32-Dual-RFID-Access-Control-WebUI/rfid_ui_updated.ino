#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WebServer.h>

// --- WiFi Ayarları ---
const char* ssid = "Ents_Test";     // WiFi ağınızın adını buraya girin
const char* password = "12345678"; // WiFi şifrenizi buraya girin

// --- NeoPixel Ayarları ---
#define LED_PIN_ARGB    2     // ARGB LED'lerin bağlı olduğu pin
#define NUM_LEDS_ARGB   15    // ARGB LED sayısı
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS_ARGB, LED_PIN_ARGB, NEO_GRB + NEO_KHZ800);

// --- RFID Okuyucu 1 Ayarları ---
#define RST_PIN_1   22    // RFID 1 Reset Pini
#define SS_PIN_1    15    // RFID 1 SS (SDA) Pini
MFRC522 rfid1(SS_PIN_1, RST_PIN_1);

// --- RFID Okuyucu 2 Ayarları ---
#define RST_PIN_2   21    // RFID 2 Reset Pini
#define SS_PIN_2    4     // RFID 2 SS (SDA) Pini
MFRC522 rfid2(SS_PIN_2, RST_PIN_2);

// --- İzinli Kart UID'leri ---
const byte allowedUIDs_Reader1[][4] = {
  {0xFA, 0x44, 0xA9, 0x00},
  {0x71, 0x83, 0xA1, 0x27}
};
const int numAllowedUIDs_Reader1 = sizeof(allowedUIDs_Reader1) / sizeof(allowedUIDs_Reader1[0]);

const byte allowedUIDs_Reader2[][4] = {
  {0x75, 0x41, 0x61, 0x9A},
  {0xC9, 0x51, 0x82, 0x83}
};
const int numAllowedUIDs_Reader2 = sizeof(allowedUIDs_Reader2) / sizeof(allowedUIDs_Reader2[0]);

// --- Web Sunucusu ---
WebServer server(80); // Web sunucusunu 80 portunda başlat

// --- Web Arayüzü İçin Global Değişkenler ---
String lastUID = "Henuz Kart Okutulmadi";
String lastStatus = "-";
int lastReader = 0;

// --- Fonksiyon Prototipleri ---
void printCardUID(MFRC522::Uid uid);
bool checkUID(MFRC522::Uid uid, int readerNumber);
void feedbackARGB(bool success);
void handleRoot();
void handleNotFound();
String getUIDString(MFRC522::Uid uid);

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("RFID Kart Esleştirme Testi (Web Arayüzlü)");
  Serial.println("----------------------------------------");

  // --- ARGB LED Başlatma ---
  strip.begin();
  strip.setBrightness(50);
  strip.fill(strip.Color(0, 0, 0), 0, NUM_LEDS_ARGB);
  strip.show();
  Serial.println("ARGB LED Seridi Baslatildi.");

  // --- SPI Başlatma ---
  SPI.begin();

  // --- RFID Okuyucuları Başlatma ---
  Serial.println("Okuyucu 1 Baslatiliyor...");
  rfid1.PCD_Init();
  byte version1 = rfid1.PCD_ReadRegister(MFRC522::VersionReg);
  if (version1 == 0x00 || version1 == 0xFF) {
    Serial.println("UYARI: Okuyucu 1 baslatilamadi!");
  } else {
    Serial.print("Okuyucu 1 Versiyon: 0x"); Serial.println(version1, HEX);
  }

  Serial.println("Okuyucu 2 Baslatiliyor...");
  rfid2.PCD_Init();
  byte version2 = rfid2.PCD_ReadRegister(MFRC522::VersionReg);
  if (version2 == 0x00 || version2 == 0xFF) {
    Serial.println("UYARI: Okuyucu 2 baslatilamadi!");
  } else {
    Serial.print("Okuyucu 2 Versiyon: 0x"); Serial.println(version2, HEX);
  }

  // --- WiFi Bağlantısı ---
  Serial.println("\nWiFi Agina Baglaniliyor...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Baglandi!");
  Serial.print("IP Adresi: ");
  Serial.println(WiFi.localIP());

  // --- Web Sunucusu Rotaları ---
  server.on("/", handleRoot); // Ana sayfa
  server.onNotFound(handleNotFound); // Bulunamayan sayfalar
  server.begin(); // Web sunucusunu başlat
  Serial.println("Web Sunucusu Baslatildi.");

  Serial.println(F("\nOkuyucular Hazir. Kartlari okutun..."));
  Serial.println("----------------------------------------");
}

void loop() {
  server.handleClient(); // Web sunucusu isteklerini dinle

  bool cardMatchStatus;

  // --- Okuyucu 1 Kontrolü ---
  if (rfid1.PICC_IsNewCardPresent() && rfid1.PICC_ReadCardSerial()) {
    Serial.print("Okuyucu 1 <<<--- ");
    printCardUID(rfid1.uid);
    lastUID = getUIDString(rfid1.uid); // UID'yi güncelle
    lastReader = 1; // Okuyucuyu güncelle
    cardMatchStatus = checkUID(rfid1.uid, 1);
    lastStatus = cardMatchStatus ? "Giris Basarili!" : "Giris Reddedildi!"; // Durumu güncelle
    feedbackARGB(cardMatchStatus);
    rfid1.PICC_HaltA();
  }

  // --- Okuyucu 2 Kontrolü ---
  if (rfid2.PICC_IsNewCardPresent() && rfid2.PICC_ReadCardSerial()) {
    Serial.print("Okuyucu 2 --->>> ");
    printCardUID(rfid2.uid);
    lastUID = getUIDString(rfid2.uid); // UID'yi güncelle
    lastReader = 2; // Okuyucuyu güncelle
    cardMatchStatus = checkUID(rfid2.uid, 2);
    lastStatus = cardMatchStatus ? "Giris Basarili!" : "Giris Reddedildi!"; // Durumu güncelle
    feedbackARGB(cardMatchStatus);
    rfid2.PICC_HaltA();
  }
  delay(50); // Kısa bir gecikme
}

// --- UID'yi String'e Çevir ---
String getUIDString(MFRC522::Uid uid) {
  String uidStr = "";
  for (byte i = 0; i < uid.size; i++) {
    uidStr += (uid.uidByte[i] < 0x10 ? " 0" : " ");
    uidStr += String(uid.uidByte[i], HEX);
  }
  uidStr.toUpperCase();
  return uidStr.substring(1); // Baştaki boşluğu kaldır
}

// --- UID'yi Seri Monitöre Yazdır ---
void printCardUID(MFRC522::Uid uid) {
  Serial.print(F("Kart UID:"));
  Serial.print(getUIDString(uid));
}

// --- UID Kontrolü ---
bool checkUID(MFRC522::Uid uid, int readerNumber) {
  bool match = false;
  const byte (*allowedUIDs)[4];
  int numAllowedUIDs;

  if (readerNumber == 1) {
    allowedUIDs = allowedUIDs_Reader1;
    numAllowedUIDs = numAllowedUIDs_Reader1;
  } else if (readerNumber == 2) {
    allowedUIDs = allowedUIDs_Reader2;
    numAllowedUIDs = numAllowedUIDs_Reader2;
  } else {
    return false;
  }

  if (uid.size != 4) {
    Serial.println(" (Gecersiz UID boyutu!)");
    return false;
  }

  for (int i = 0; i < numAllowedUIDs; i++) {
    if (memcmp(uid.uidByte, allowedUIDs[i], 4) == 0) {
      match = true;
      break;
    }
  }

  if (match) {
    Serial.println(" (DOGRU KART!) - Giris Basarili!");
  } else {
    Serial.println(" (YANLIS KART!)");
  }
  return match;
}

// --- ARGB LED Geri Bildirimi ---
void feedbackARGB(bool success) {
  uint32_t color = success ? strip.Color(0, 150, 0) : strip.Color(150, 0, 0);
  Serial.println(success ? "ARGB Feedback: YESIL" : "ARGB Feedback: KIRMIZI");

  for (int i = 0; i < 2; i++) { // 3 yerine 2 kez yanıp sönsün, daha hızlı olsun
    strip.fill(color, 0, NUM_LEDS_ARGB);
    strip.show();
    delay(200);
    strip.fill(strip.Color(0, 0, 0), 0, NUM_LEDS_ARGB);
    strip.show();
    delay(200);
  }
}

// --- Web Ana Sayfası İşleyicisi ---
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><title>RFID</title>";
  html += "<meta http-equiv='refresh' content='5'>"; // Sayfayı 5 saniyede bir yenile
  html += "<style>";
  html += "body { font-family: sans-serif; background-color: #f4f4f4; text-align: center; margin-top: 50px; }";
  html += ".container { background-color: #fff; padding: 30px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); display: inline-block; }";
  html += "h1 { color: #333; }";
  html += "p { font-size: 1.2em; color: #555; }";
  html += "span { font-weight: bold; padding: 5px 10px; border-radius: 4px; color: #fff; }";
  html += ".success { background-color: #28a745; }";
  html += ".fail { background-color: #dc3545; }";
  html += ".neutral { background-color: #6c757d; }";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>ESP32 RFID Web Arayuzu</h1>";
  html += "<p>Son Okunan Kart UID: <b>" + lastUID + "</b></p>";
  html += "<p>Okuyucu: <b>" + String(lastReader == 0 ? "-" : String(lastReader)) + "</b></p>";
  
  String statusClass = "neutral";
  if (lastStatus.indexOf("Basarili") != -1) statusClass = "success";
  else if (lastStatus.indexOf("Reddedildi") != -1) statusClass = "fail";

  html += "<p>Durum: <span class='" + statusClass + "'>" + lastStatus + "</span></p>";
  html += "</div></body></html>";
  
  server.send(200, "text/html", html); // HTML sayfasını gönder
}

// --- Bulunamayan Sayfa İşleyicisi ---
void handleNotFound() {
  server.send(404, "text/plain", "404: Sayfa Bulunamadi");
}