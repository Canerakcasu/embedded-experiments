#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32Encoder.h>
#include <MD_Parola.h>      // <<< MATRIX IÇIN EKLENDI
#include <MD_MAX72xx.h>     
#include <SPI.h>            

// --- Ağ Ayarları ---
const char* ssid = "Ents_Test";
const char* password = "12345678";
IPAddress local_IP(192, 168, 20, 21);
IPAddress gateway(192, 168, 20, 1);
IPAddress subnet(255, 255, 255, 0);

// --- Pin Tanımları ---
#define ARGB_LED_PIN 33
#define ENCODER_CLK_PIN 32
#define ENCODER_DT_PIN  35
#define ENCODER_SW_PIN  34

// --- LED Ayarları ---
#define TOTAL_LEDS 15
#define LEDS_PER_GROUP 3
#define NUM_GROUPS (TOTAL_LEDS / LEDS_PER_GROUP) // Sonuç 5

Adafruit_NeoPixel strip(TOTAL_LEDS, ARGB_LED_PIN, NEO_GRB + NEO_KHZ800);
ESP32Encoder encoder;
WebServer server(80);

// --- MAX7219 Pin Tanımlamaları ---
#define MAX_HW_TYPE MD_MAX72XX::FC16_HW
#define MAX_MATRIX_DEVICES 4
#define MATRIX_CLK_PIN     2
#define MATRIX_DATA_PIN    12
#define MATRIX_CS_PIN      13

MD_Parola P = MD_Parola(MAX_HW_TYPE, MATRIX_DATA_PIN, MATRIX_CLK_PIN, MATRIX_CS_PIN, MAX_MATRIX_DEVICES);
char webMatrixMessage[100] = "ESP32 Matrix!"; // Web'den gelen mesajı tutacak
bool newWebMatrixMessageAvailable = true;    // Yeni mesaj var mı?

// --- Durum Değişkenleri ---
bool argbGroupState[NUM_GROUPS] = {true, true, true, true, true};
uint32_t groupColors[NUM_GROUPS];
int currentBrightness = 75;
bool globalOn = true;

String currentTheme = "dark";
long oldEncoderPosition = 0;

// --- Yardımcı Fonksiyonlar ---
String colorToHex(uint32_t color) {
  char hexColor[8];
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;
  sprintf(hexColor, "#%02X%02X%02X", r, g, b);
  return String(hexColor);
}
uint32_t hexToColor(String hex) {
  if (hex.startsWith("#")) { hex = hex.substring(1); }
  if (hex.length() != 6) { return 0; }
  long number = strtol(hex.c_str(), NULL, 16);
  return strip.Color((number >> 16) & 0xFF, (number >> 8) & 0xFF, number & 0xFF);
}
uint32_t adjustBrightness(uint32_t baseColor, int brightnessPercentage) {
  if (brightnessPercentage < 0) brightnessPercentage = 0;
  if (brightnessPercentage > 100) brightnessPercentage = 100;
  if (brightnessPercentage == 0) return strip.Color(0, 0, 0);
  uint8_t r_comp = (baseColor >> 16) & 0xFF; uint8_t g_comp = (baseColor >> 8) & 0xFF; uint8_t b_comp = baseColor & 0xFF;
  r_comp = (r_comp * brightnessPercentage) / 100; g_comp = (g_comp * brightnessPercentage) / 100; b_comp = (b_comp * brightnessPercentage) / 100;
  return strip.Color(r_comp, g_comp, b_comp);
}

void updateLeds() {
  Serial.println("Updating LEDs. Global ON: " + String(globalOn) + ", Brightness: " + String(currentBrightness));
  for (int g = 0; g < NUM_GROUPS; g++) {
    for (int i = 0; i < LEDS_PER_GROUP; i++) {
      int ledIndex = g * LEDS_PER_GROUP + i;
      if (ledIndex < TOTAL_LEDS) {
        if (globalOn && argbGroupState[g]) {
          uint32_t baseColor = groupColors[g];
          strip.setPixelColor(ledIndex, adjustBrightness(baseColor, currentBrightness));
        } else {
          strip.setPixelColor(ledIndex, 0);
        }
      }
    }
  }
  strip.show();
}

// HTML Sayfa Oluşturucu
String buildHtml() {
  String bgColor = currentTheme == "dark" ? "#1e1e1e" : "#f4f4f4";
  String textColor = currentTheme == "dark" ? "#e0e0e0" : "#333333";
  String cardBgColor = currentTheme == "dark" ? "#2c2c2c" : "#ffffff";
  String cardBorder = currentTheme == "dark" ? "1px solid #444" : "1px solid #ddd";

  String html = F("<!DOCTYPE html><html lang='tr'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>");
  // Otomatik yenileme kaldırıldı
  html += F("<title>ESP32 ARGB Kontrol</title><style>");
  html += F("body{font-family:'Segoe UI',Tahoma,Geneva,Verdana,sans-serif;background-color:");
  html += bgColor; html += F(";color:"); html += textColor; html += F(";text-align:center;padding:20px;margin:0;}");
  html += F("h1{color:"); html += (currentTheme == "dark" ? F("#bb86fc") : F("#007bff")); html += F("; margin-bottom:30px;}");
  html += F(".container{max-width:700px; margin:auto; padding:20px;}");
  html += F(".card{background-color:"); html += cardBgColor; html += F("; border:"); html += cardBorder; html += F("; border-radius:8px; padding:20px; margin-bottom:20px; box-shadow: 0 2px 4px rgba(0,0,0,0.1);}");
  html += F(".button{padding:10px 18px;margin:5px;font-size:15px;border:none;border-radius:5px;cursor:pointer;transition:background-color 0.3s ease;}");
  html += F(".on{background-color:#28a745;color:white;} .on:hover{background-color:#218838;}");
  html += F(".off{background-color:#dc3545;color:white;} .off:hover{background-color:#c82333;}");
  html += F(".theme{background-color:#007bff;color:white;} .theme:hover{background-color:#0056b3;}");
  html += F(".color-set{background-color:#6c757d;color:white;} .color-set:hover{background-color:#5a6268;}");
  html += F(".global-color-form{display:flex; align-items:center; justify-content:center; margin-top:15px;}");
  html += F(".group-control{display:flex; flex-wrap:wrap; align-items:center; justify-content:space-between; margin-bottom:10px; padding:10px; border-bottom:");
  html += cardBorder; html += F(";}");
  html += F(".group-control:last-child{border-bottom:none;}");
  html += F("label{margin-right:10px;}");
  html += F("input[type='color']{width:50px; height:30px; border:none; padding:0; border-radius:4px; cursor:pointer; vertical-align:middle;}");
  html += F("input[type='text']{padding:8px; border-radius:4px; border:1px solid #ccc; margin-right:5px; flex-grow:1; min-width:150px;}"); // Matrix input için stil
  html += F(".status-text{font-size:18px; margin-top:10px;}");
  html += F("</style></head><body><div class='container'>");
  html += F("<h1>ESP32 ARGB LED & Matrix Kontrol</h1>");

  html += F("<div class='card'>");
  html += F("<h2>Genel Ayarlar</h2>");
  html += F("<form action='/toggleGlobalOnOff' method='POST' style='display:inline-block; margin-right:10px;'>");
  html += F("<button class='button "); html += globalOn ? F("on") : F("off");
  html += F("' type='submit'>"); html += globalOn ? F("Tüm LED'leri KAPAT") : F("Tüm LED'leri AÇ");
  html += F("</button></form>");
  html += F("<form action='/toggleTheme' method='POST' style='display:inline-block;'><button class='button theme' type='submit'>Temayı Değiştir (");
  html += currentTheme; html += F(")</button></form>");
  html += F("<div class='status-text'>Genel Durum: "); html += globalOn ? F("AÇIK") : F("KAPALI"); html += F("</div>");
  html += F("<div class='status-text'>Parlaklık: "); html += String(currentBrightness); html += F("%</div>");
  html += F("<form action='/setAllColor' method='POST' class='global-color-form'>");
  html += F("<label for='global_color'>Tüm Gruplar İçin Ortak Renk:</label>");
  html += F("<input type='color' id='global_color' name='global_color_hex' value='#FFFFFF' title='Tüm Gruplar İçin Renk'>");
  html += F("<button class='button color-set' type='submit' style='margin-left:10px;'>Tümüne Uygula</button></form>");
  html += F("</div>");

  html += F("<div class='card'>");
  html += F("<h2>Grup Kontrolleri (İlk 2 Grup)</h2>");
  // Sadece ilk 2 grup için kontrol oluştur
  for (int g = 0; g < 2; g++) { // <<< DÖNGÜ 2'YE KADAR GİDİYOR
    if (g < NUM_GROUPS) { // Güvenlik kontrolü
        html += F("<div class='group-control'>");
        html += F("<span style='margin-right:auto;'>Grup "); html += String(g + 1); html += F("</span>");
        html += F("<div>");
        html += F("<form action='/setGroupColor' method='POST' style='display:inline-flex; align-items:center; margin-right:10px;'>");
        html += F("<input type='hidden' name='group_idx' value='"); html += String(g); html += F("'>");
        html += F("<input type='color' name='color_hex' value='"); html += colorToHex(groupColors[g]);
        html += F("' title='Grup "); html += String(g+1); html += F(" Renk'>");
        html += F("<button class='button color-set' type='submit' style='margin-left:5px;'>Ayarla</button></form>");
        html += F("<form action='/toggleGroup?g="); html += String(g);
        html += F("' method='POST' style='display:inline-block;'>");
        html += F("<button class='button "); html += String(argbGroupState[g] ? F("on") : F("off"));
        html += F("' type='submit'>"); html += String(argbGroupState[g] ? F("Kapat") : F("Aç"));
        html += F("</button></form>");
        html += F("</div></div>");
    }
  }
  html += F("</div>");

  // Matrix Ekrana Yazı Gönderme
  html += F("<div class='card'><h2>Matrix Ekrana Yazı Gönder</h2>");
  html += F("<form action='/setMatrixText' method='POST' style='display:flex;'>");
  html += F("<input type='text' name='matrix_text' placeholder='Matrixe gönderilecek yazı...' value='");
  html += String(webMatrixMessage); 
  html += F("'>");
  html += F("<button class='button color-set' type='submit' style='margin-left:5px;'>Gönder</button></form></div>");
  
  html += F("</div></body></html>");
  return html;
}

// Web Handler
void handleRoot() { server.send(200, "text/html", buildHtml()); }
void handleToggleGroup() { if (server.hasArg("g")) { int g = server.arg("g").toInt(); if (g >= 0 && g < NUM_GROUPS) { argbGroupState[g] = !argbGroupState[g]; updateLeds(); } } server.sendHeader("Location", "/"); server.send(303); }
void handleToggleTheme() { currentTheme = (currentTheme == "dark" ? "light" : "dark"); server.sendHeader("Location", "/"); server.send(303); }
void handleSetGroupColor() { if (server.hasArg("group_idx") && server.hasArg("color_hex")) { int g = server.arg("group_idx").toInt(); String colorHex = server.arg("color_hex"); if (g >= 0 && g < NUM_GROUPS) { groupColors[g] = hexToColor(colorHex); updateLeds(); } } server.sendHeader("Location", "/"); server.send(303); }
void handleToggleGlobalOnOff() { globalOn = !globalOn; updateLeds(); server.sendHeader("Location", "/"); server.send(303); }
void handleSetAllColor() { if (server.hasArg("global_color_hex")) { String colorHex = server.arg("global_color_hex"); uint32_t newColor = hexToColor(colorHex); for (int g = 0; g < NUM_GROUPS; g++) { groupColors[g] = newColor; } updateLeds(); } server.sendHeader("Location", "/"); server.send(303); }

// --- Matrix
void handleSetMatrixText() {
  if (server.hasArg("matrix_text")) {
    String newText = server.arg("matrix_text");
    strncpy(webMatrixMessage, newText.c_str(), 99); 
    webMatrixMessage[99] = '\0'; 
    newWebMatrixMessageAvailable = true; 
    Serial.print("New matrix text received: "); Serial.println(webMatrixMessage);
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  Serial.begin(115200);

  for (int g = 0; g < NUM_GROUPS; g++) {
    groupColors[g] = strip.Color( (g * 50 + 50) % 256, (200 - g * 40) % 256, (100 + g * 30) % 256 );
  }

  strip.begin(); strip.setBrightness(255);

  // --- MAX7219 (Parola)
  P.begin();
  P.setIntensity(4); 
  P.displayClear();
  P.setTextAlignment(PA_LEFT);
  P.setTextEffect(PA_SCROLL_LEFT, PA_SCROLL_LEFT);
  P.setSpeed(75);  
  P.setPause(0);   
  // P.displayText(webMatrixMessage, PA_LEFT, 75, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);

  pinMode(ENCODER_CLK_PIN, INPUT); pinMode(ENCODER_DT_PIN, INPUT); pinMode(ENCODER_SW_PIN, INPUT);
  encoder.attachHalfQuad(ENCODER_DT_PIN, ENCODER_CLK_PIN);
  encoder.setCount(currentBrightness); oldEncoderPosition = currentBrightness;

  Serial.print("Statik IP ayarlanıyor...");
  if (!WiFi.config(local_IP, gateway, subnet)) { Serial.println("Statik IP ayarlanamadı!"); }
  WiFi.begin(ssid, password);
  Serial.print("WiFi bağlanıyor");
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 30) { delay(500); Serial.print("."); retries++; }
  Serial.println("");
  if (WiFi.status() == WL_CONNECTED) { Serial.print("WiFi bağlı. IP adresi: "); Serial.println(WiFi.localIP()); }
  else { Serial.println("WiFi bağlantısı başarısız. SoftAP moduna geçiliyor..."); WiFi.softAP("ESP32-FallbackAP", "12345678"); Serial.print("AP IP adresi: "); Serial.println(WiFi.softAPIP()); }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/toggleGroup", HTTP_POST, handleToggleGroup);
  server.on("/toggleTheme", HTTP_POST, handleToggleTheme);
  server.on("/setGroupColor", HTTP_POST, handleSetGroupColor);
  server.on("/toggleGlobalOnOff", HTTP_POST, handleToggleGlobalOnOff);
  server.on("/setAllColor", HTTP_POST, handleSetAllColor);
  server.on("/setMatrixText", HTTP_POST, handleSetMatrixText); // Matrix handler'ı kaydet
  server.begin();
  Serial.println("Web sunucusu başlatıldı.");
  updateLeds();
}

void loop() {
  server.handleClient();

  // --- MAX7219 (Parola) Animasyon ---
  if (P.displayAnimate()) { 
    if (newWebMatrixMessageAvailable) { 
      P.displayText(webMatrixMessage, PA_LEFT, 75, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
      newWebMatrixMessageAvailable = false;
    } else { 
      P.displayText(webMatrixMessage, PA_LEFT, 75, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
    }
  }

  long val = encoder.getCount();
  if (val < 0) { val = 0; encoder.setCount(0); }
  if (val > 100) { val = 100; encoder.setCount(100); }
  if (val != currentBrightness) {
    currentBrightness = val;
    updateLeds();
  }
  
  delay(50);
}