#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_NeoPixel.h> 


#define LED_PIN_ARGB    2    
#define NUM_LEDS_ARGB   15   
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS_ARGB, LED_PIN_ARGB, NEO_GRB + NEO_KHZ800);

-
#define RST_PIN_1   22
#define SS_PIN_1    15


#define RST_PIN_2   21
#define SS_PIN_2    4

MFRC522 rfid1(SS_PIN_1, RST_PIN_1);
MFRC522 rfid2(SS_PIN_2, RST_PIN_2);


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


void printCardUID(MFRC522::Uid uid, int readerNumber);
bool checkUID(MFRC522::Uid uid, int readerNumber);
void feedbackARGB(bool success); 

void setup() {
  Serial.begin(115200);
  delay(1000); 

  Serial.println("RFID Kart Esleştirme Testi (ARGB LED ile)");
  Serial.println("---------------------------");

  
  strip.begin();
  strip.setBrightness(50); 
  strip.fill(strip.Color(0,0,0), 0, NUM_LEDS_ARGB); 
  strip.show();
  Serial.println("ARGB LED Seridi (GPIO " + String(LED_PIN_ARGB) + ") Baslatildi.");

  SPI.begin();         

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

  Serial.println(F("\nOkuyucular Hazir. Kartlari okutun..."));
  Serial.println("---------------------------");
}

void loop() {
  bool cardMatchStatus;

 
  if (rfid1.PICC_IsNewCardPresent() && rfid1.PICC_ReadCardSerial()) {
    Serial.print("Okuyucu 1 <<<--- ");
    printCardUID(rfid1.uid, 1);
    cardMatchStatus = checkUID(rfid1.uid, 1);
    feedbackARGB(cardMatchStatus); 
    rfid1.PICC_HaltA();      
  }


  if (rfid2.PICC_IsNewCardPresent() && rfid2.PICC_ReadCardSerial()) {
    Serial.print("Okuyucu 2 --->>> ");
    printCardUID(rfid2.uid, 2);
    cardMatchStatus = checkUID(rfid2.uid, 2);
    feedbackARGB(cardMatchStatus); 
    rfid2.PICC_HaltA();      
  }
  delay(100); 
}


void printCardUID(MFRC522::Uid uid, int readerNumber) {
  Serial.print(F("Kart UID:"));
  for (byte i = 0; i < uid.size; i++) {
     Serial.print(uid.uidByte[i] < 0x10 ? " 0" : " "); 
     Serial.print(uid.uidByte[i], HEX); 
  }
}


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


void feedbackARGB(bool success) {
  uint32_t color;
  if (success) {
    color = strip.Color(0, 150, 0); 
    Serial.println("ARGB Feedback: YESIL");
  } else {
    color = strip.Color(150, 0, 0); 
    Serial.println("ARGB Feedback: KIRMIZI");
  }

  for (int i = 0; i < 3; i++) { 
    strip.fill(color, 0, NUM_LEDS_ARGB); 
    strip.show();
    delay(300);
    strip.fill(strip.Color(0,0,0), 0, NUM_LEDS_ARGB); 
    strip.show();
    delay(300);
  }
}