
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <PubSubClient.h>
#include "DFRobotDFPlayerMini.h"
#include <FastLED.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

// --- LED Şerit Ayarları ---
#define LED_STRIP_PIN 2
#define NUM_LEDS_STRIP 15
CRGB leds[NUM_LEDS_STRIP];
#define STRIP_BRIGHTNESS 50
const CRGB STRIP_BG_COLOR = CRGB::DarkGreen;
const CRGB SPOT_COLOR = CRGB::Red;

// --- LED Matrix Ayarları ---
#define MATRIX_HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MATRIX_MAX_DEVICES 4
#define MATRIX_CLK_PIN  33
#define MATRIX_DATA_PIN 14
#define MATRIX_CS_PIN   27
MD_Parola P = MD_Parola(MATRIX_HARDWARE_TYPE, MATRIX_DATA_PIN, MATRIX_CLK_PIN, MATRIX_CS_PIN, MATRIX_MAX_DEVICES);
#define MATRIX_INTENSITY 2
#define MATRIX_TEXT_SCROLL_SPEED 75

// --- Buton Ayarları ---
#define BUTTON_PIN 32

// --- DFPlayer Mini Ayarları ---
HardwareSerial mp3Serial(2);
DFRobotDFPlayerMini myDFPlayer;
bool dfPlayerOnline = false;
const uint8_t SOUND_WELCOME = 1;
const uint8_t SOUND_STEP1_END = 2;
uint8_t currentTrack = 0;

// --- Oyun Değişkenleri Adım 1 ---
const char TARGET_WORD[] = "DEAD";
const int MAX_LEVELS_STEP1 = strlen(TARGET_WORD);
int currentStep1Level = 0;
char displayedWordOnMatrix[MAX_LEVELS_STEP1 + 1] = "";
bool spotActive = false;
int spotPosition = -1;
unsigned long spotAppearTime = 0;
unsigned long nextSpotTime = 0;
const unsigned long SPOT_DURATION_MS = 2000; 
const unsigned long DELAY_AFTER_CATCH_MS = 700;
const unsigned long DELAY_AFTER_MISS_MS = 1200;

// --- Genel Oyun Durumu ---
enum GamePlayState { 
    STATE_IDLE, STATE_STEP1_INITIALIZING, STATE_STEP1_WELCOME_SOUND,
    STATE_STEP1_PLAYING_GAME, STATE_STEP1_ENDING_SOUND, STATE_STEP1_COMPLETED
};
GamePlayState currentGameState = STATE_IDLE;
unsigned long stateEntryTime = 0;

// --- Buton Durumu ---
int buttonVal = HIGH;
int lastButtonVal = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
bool buttonJustPressedFlag = false;

// --- WiFi ve MQTT ---
const char* ssid = "Ents_Test";
const char* password = "12345678";
const char* mqtt_server = "192.168.20.208";
const int mqtt_port = 1883; // DÜZELTİLDİ: MQTT Portu eklendi
const char* mqtt_topic_subscribe = "game/control"; // Node-RED'den komut alacağımız topic
const char* mqtt_topic_publish = "game/status";   // DÜZELTİLDİ: ESP32'den Node-RED'e durum göndereceğimiz topic eklendi
String esp32_client_id = "ESP32_Oyun_Web_";     // Client ID güncellendi

WiFiClient espClient; 
PubSubClient client(espClient); 

// --- Web Sunucusu ---
AsyncWebServer server(80);
String processor(const String& var); 

// Fonksiyon Prototypleri
void updateMatrixDisplay(const char* textToDisplay);
void setStripBackground();
void showSpotOnStrip(int pos);
void clearStripToBackground();
void setup_wifi(); 
void reconnect_mqtt(); 
void mqtt_callback(char* topic, byte* payload, unsigned int length); 

// HTML Sayfası (index_html - önceki kodla aynı, buraya tekrar eklemiyorum, çok uzun)
// Lütfen bir önceki yanıttaki index_html[] PROGMEM = R"rawliteral(...)rawliteral"; kısmını
// kodunuzda bulundurduğunuzdan emin olun.
// ... (index_html içeriği burada olmalı) ...
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>ESP32 Oyun Kontrol Paneli</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; margin: 0; padding: 10px; background-color: #f4f4f4; color: #333; }
    .container { max-width: 600px; margin: auto; background-color: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }
    h2 { color: #007bff; }
    .section { margin-bottom: 20px; padding: 15px; border: 1px solid #ddd; border-radius: 5px; }
    .status-item { margin-bottom: 8px; }
    .status-label { font-weight: bold; }
    button, input[type="submit"], input[type="number"] { padding: 8px 12px; margin: 5px 2px; border: none; border-radius: 4px; cursor: pointer; }
    button, input[type="submit"] { background-color: #007bff; color: white; }
    button:hover, input[type="submit"]:hover { background-color: #0056b3; }
    input[type="number"] { width: 60px; }
    .controls button { min-width: 80px; }
    #trackNum { margin-right: 10px;}
  </style>
  <script>
    function sendCmd(action, value = '') {
      var xhr = new XMLHttpRequest();
      var url = "/control_dfplayer?action=" + action;
      if (action === 'play' || action === 'volume') {
        url += "&value=" + value;
      }
      xhr.open("GET", url, true);
      xhr.send();
    }
  </script>
</head><body>
  <div class="container">
    <h2>ESP32 Kontrol Paneli</h2>
    <div class="section">
      <h3>Durum Bilgileri</h3>
      <div class="status-item"><span class="status-label">WiFi Baglantisi: </span><span id="wifiStatus">%WIFI_STATUS%</span></div>
      <div class="status-item"><span class="status-label">IP Adresi: </span><span id="ipAddress">%IP_ADDRESS%</span></div>
      <div class="status-item"><span class="status-label">DFPlayer: </span><span id="dfPlayerStatus">%DFPLAYER_STATUS%</span></div>
      <div class="status-item"><span class="status-label">SD Kart: </span><span id="sdStatus">%SD_STATUS%</span></div>
      <div class="status-item"><span class="status-label">Mevcut Oyun Durumu: </span><span>%GAME_STATE%</span></div>
    </div>
    <div class="section">
      <h3>DFPlayer Ses Kontrolleri</h3>
      <div class="controls">
        Parca No: <input type="number" id="trackNum" min="1" max="255" value="1">
        <button onclick="sendCmd('play', document.getElementById('trackNum').value)">Oynat</button><br>
        <button onclick="sendCmd('stop')">Durdur</button>
        <button onclick="sendCmd('next')">Sonraki</button>
        <button onclick="sendCmd('prev')">Önceki</button><br>
        <button onclick="sendCmd('vol_up')">Sesi Artir</button>
        <button onclick="sendCmd('vol_down')">Sesi Azalt</button>
        Yeni Ses Seviyesi (0-30): <input type="number" id="volumeValue" min="0" max="30" value="24">
        <button onclick="sendCmd('volume', document.getElementById('volumeValue').value)">Ayarla</button>
      </div>
    </div>
    <p><a href="/"><button>Sayfayi Yenile</button></a></p>
  </div>
</body></html>
)rawliteral";


// HTML Template İşlemcisi (Önceki kodla aynı)
String processor(const String& var){
  if(var == "WIFI_STATUS"){ return (WiFi.status() == WL_CONNECTED) ? "Bagli" : "Bagli Değil / Baglaniyor..."; }
  if(var == "IP_ADDRESS"){ return (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "N/A"; }
  if(var == "DFPLAYER_STATUS"){ return dfPlayerOnline ? "Online" : "Offline/Hata"; }
  if(var == "SD_STATUS"){ return dfPlayerOnline ? "Hazir (Muhtemelen Takili)" : "Takili Değil / Okunamiyor"; }
  if(var == "GAME_STATE"){
    switch(currentGameState){
      case STATE_IDLE: return "Beklemede (IDLE)";
      case STATE_STEP1_INITIALIZING: return "Adim 1 Baslatiliyor";
      case STATE_STEP1_WELCOME_SOUND: return "Adim 1 Karşilama Sesi";
      case STATE_STEP1_PLAYING_GAME: return "Adim 1 Oynaniyor";
      case STATE_STEP1_ENDING_SOUND: return "Adim 1 Bitiş Sesi";
      case STATE_STEP1_COMPLETED: return "Adim 1 Tamamlandi";
      default: return "Bilinmiyor";
    }
  }
  return String();
}

// --- SETUP ---
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n\n======== ESP32 OYUNU - ADIM 1 (WEB ARAYUZU - MQTT Topic Duzeltildi) ========");
    randomSeed(analogRead(0));

    // Donanım başlatmaları (LED Şerit, Matrix, DFPlayer, Buton - önceki kodla aynı)
    FastLED.addLeds<WS2812B, LED_STRIP_PIN, GRB>(leds, NUM_LEDS_STRIP).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(STRIP_BRIGHTNESS);
    fill_solid(leds, NUM_LEDS_STRIP, CRGB::Black); FastLED.show();
    Serial.println("FastLED baslatildi.");

    P.begin(); P.setIntensity(MATRIX_INTENSITY); P.displayClear();
    updateMatrixDisplay("WEB OK"); delay(1000); updateMatrixDisplay(""); // INIT yerine WEB OK
    Serial.println("MD_Parola baslatildi.");

    mp3Serial.begin(9600, SERIAL_8N1, 16, 17);
    Serial.println("DFPlayer Seri Port baslatildi.");
    if (!myDFPlayer.begin(mp3Serial, true, false)) {
        Serial.println(F("HATA: myDFPlayer.begin() basarisiz oldu!")); dfPlayerOnline = false;
    } else {
        Serial.println(F("BASARILI: DFPlayer Mini online."));
        myDFPlayer.volume(24); dfPlayerOnline = true;
    }
    Serial.println("DFPlayer Online: " + String(dfPlayerOnline ? "EVET" : "HAYIR"));

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    Serial.println("Button (GPIO" + String(BUTTON_PIN) + ") ayarlandi.");
    
    // WiFi Bağlantısı
    setup_wifi(); 

    // MQTT 
    if (WiFi.status() == WL_CONNECTED) {
        client.setServer(mqtt_server, mqtt_port); // mqtt_port burada kullanılıyor
        client.setCallback(mqtt_callback);
        Serial.println("MQTT server ve callback ayarlandi."); // YENİ LOG
    } else {
        Serial.println("WiFi bagli degil, MQTT ayarlari atlandi."); // YENİ LOG
    }

    // Web Sunucusu Rotaları (Önceki kodla aynı)
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html, processor);
    });
    server.on("/control_dfplayer", HTTP_GET, [] (AsyncWebServerRequest *request) {
        String action = ""; String value_str = ""; int track = 0; int volume_val = 0;
        if (request->hasParam("action")) {
            action = request->getParam("action")->value();
            Serial.print("Web'den DFPlayer komutu: action="); Serial.println(action);
            if (dfPlayerOnline) {
                if (action == "play") { if (request->hasParam("value")) { track = request->getParam("value")->value().toInt(); if (track > 0) { myDFPlayer.play(track); currentTrack = track; Serial.print("Web: Parca caliniyor: "); Serial.println(track);}}}
                else if (action == "stop") { myDFPlayer.stop(); currentTrack = 0; Serial.println("Web: DFPlayer durduruldu.");}
                else if (action == "next") { myDFPlayer.next(); currentTrack = 0; Serial.println("Web: Sonraki parca.");}
                else if (action == "prev") { myDFPlayer.previous(); currentTrack = 0; Serial.println("Web: Onceki parca.");}
                else if (action == "vol_up") { myDFPlayer.volumeUp(); Serial.println("Web: Ses artirildi.");}
                else if (action == "vol_down") { myDFPlayer.volumeDown(); Serial.println("Web: Ses azaltildi.");}
                else if (action == "volume") { if (request->hasParam("value")) { volume_val = request->getParam("value")->value().toInt(); if (volume_val >=0 && volume_val <=30) { myDFPlayer.volume(volume_val); Serial.print("Web: Ses ayarlandi: "); Serial.println(volume_val);}}}
                request->send(200, "text/plain", "OK: " + action);
            } else { Serial.println("Web komutu: DFPlayer offline."); request->send(503, "text/plain", "DFPlayer Offline");}
        } else { request->send(400, "text/plain", "Bad Request: No action specified");}
    });
    
    server.begin(); // Web sunucusunu başlat
    Serial.println("HTTP Web Sunucusu baslatildi.");

    currentGameState = STATE_IDLE;
    stateEntryTime = millis();
    Serial.println("Oyun durumu IDLE. ADIM 1'i BASLATMAK ICIN BUTONA BASIN veya Web Arayuzunu kullanin.");
    Serial.println("--- SETUP TAMAMLANDI ---");
}


// --- LOOP, Oyun Mantığı ve Diğer Yardımcı Fonksiyonlar ---
// Bu kısımlar bir önceki "Adım 1 (Tam ve Bağımsız Çalışan Versiyon)" koduyla aynı kalacak.
// Lütfen bir önceki yanıttaki loop(), updateMatrixDisplay(), setStripBackground(), 
// showSpotOnStrip(), clearStripToBackground(), mqtt_callback(), setup_wifi(), 
// ve reconnect_mqtt() fonksiyonlarını buraya kopyalayın.
// Sadece global değişkenler ve setup() fonksiyonunun başındaki MQTT tanımlamaları değişti.

// void loop() { ... ÖNCEKİ KODDAKİ GİBİ ... }
// void updateMatrixDisplay(const char* textToDisplay) { ... ÖNCEKİ KODDAKİ GİBİ ... }
// void setStripBackground() { ... ÖNCEKİ KODDAKİ GİBİ ... }
// void showSpotOnStrip(int pos) { ... ÖNCEKİ KODDAKİ GİBİ ... }
// void clearStripToBackground() { ... ÖNCEKİ KODDAKİ GİBİ ... }
// void mqtt_callback(char* topic, byte* payload, unsigned int length) { ... ÖNCEKİ KODDAKİ GİBİ ... } // Bu zaten yukarıda düzeltilmiş haliyle var
// void setup_wifi() { ... ÖNCEKİ KODDAKİ GİBİ ... } // Bu zaten yukarıda düzeltilmiş haliyle var
// void reconnect_mqtt() { ... ÖNCEKİ KODDAKİ GİBİ ... } // Bu zaten yukarıda düzeltilmiş haliyle var

// ÖNEMLİ: Yukarıdaki yorum satırlarını dikkate alarak, bir önceki yanıttaki
// loop() ve diğer yardımcı fonksiyonları bu kodun devamına eklemeyi unutmayın.
// Sadece bu yanıttaki global değişkenler ve setup() fonksiyonu güncellenmiştir.
// Daha net olması için loop ve diğer fonksiyonları da buraya ekliyorum:

void loop() {
    if (WiFi.status() == WL_CONNECTED) {
        if (!client.connected()) {
            static unsigned long lastMqttRetry = 0;
            if (millis() - lastMqttRetry > 5000) {
                lastMqttRetry = millis();
                reconnect_mqtt(); 
            }
        }
        if (client.connected()) client.loop(); // client.loop() sadece bağlıyken çağrılmalı
    }

    buttonJustPressedFlag = false;
    int currentReading = digitalRead(BUTTON_PIN);
    if (currentReading != lastButtonVal) { lastDebounceTime = millis(); }
    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (currentReading != buttonVal) {
            buttonVal = currentReading;
            if (buttonVal == LOW) {
                buttonJustPressedFlag = true;
                Serial.println("--> BUTON BASILDI (Fiziksel) <--");
                if(client.connected()) client.publish(mqtt_topic_publish, "{\"event\":\"physical_button_action\"}");
            }
        }
    }
    lastButtonVal = currentReading;

    if (dfPlayerOnline && myDFPlayer.available()) {
        uint8_t type = myDFPlayer.readType();
        switch (type) {
            case DFPlayerPlayFinished:
                Serial.print(F("DFPlayer: Ses bitti. Calan parca (tahmini): ")); Serial.println(currentTrack);
                currentTrack = 0; 
                if (currentGameState == STATE_STEP1_WELCOME_SOUND) { Serial.println("Karsilama Sesi bitti.");} 
                else if (currentGameState == STATE_STEP1_ENDING_SOUND) {
                    Serial.println("Adim 1 Bitis Sesi bitti.");
                    currentGameState = STATE_STEP1_COMPLETED; stateEntryTime = millis();
                }
                break;
            case DFPlayerError: Serial.print(F("DFPlayer Hata: ")); Serial.println(myDFPlayer.read()); break;
        }
    }

    switch (currentGameState) {
        case STATE_IDLE:
            if (buttonJustPressedFlag) { currentGameState = STATE_STEP1_INITIALIZING; stateEntryTime = millis(); }
            break;
        case STATE_STEP1_INITIALIZING:
            Serial.println("Adim 1 Baslatiliyor (Initializing)...");
            currentStep1Level = 0; strcpy(displayedWordOnMatrix, ""); 
            updateMatrixDisplay("----");       
            fill_solid(leds, NUM_LEDS_STRIP, CRGB::Black); FastLED.show();
            currentGameState = STATE_STEP1_WELCOME_SOUND; stateEntryTime = millis();
            break;
        case STATE_STEP1_WELCOME_SOUND:
            Serial.println("Karsilama Sesi (Track " + String(SOUND_WELCOME) + ") caliniyor...");
            if (dfPlayerOnline) { myDFPlayer.play(SOUND_WELCOME); currentTrack = SOUND_WELCOME; }
            else Serial.println("DFPlayer offline, ses calinamiyor.");
            setStripBackground(); FastLED.show();
            currentGameState = STATE_STEP1_PLAYING_GAME; stateEntryTime = millis();
            nextSpotTime = millis() + random(1500, 3000); spotActive = false;
            Serial.println("Oyun durumu: Karsilama Sesi -> Oyun Oynaniyor (Spot Bekleniyor)");
            break;
        case STATE_STEP1_PLAYING_GAME:
            if (!spotActive) { 
                if (millis() >= nextSpotTime) { 
                    spotPosition = random(NUM_LEDS_STRIP);
                    Serial.print("Yeni spot gosteriliyor, Pozisyon: "); Serial.println(spotPosition);
                    showSpotOnStrip(spotPosition); FastLED.show(); 
                    spotActive = true; spotAppearTime = millis(); 
                }
            } else { 
                if (buttonJustPressedFlag) { 
                    Serial.println("NOKTA YAKALANDI!");
                    clearStripToBackground(); FastLED.show();
                    currentStep1Level++;
                    if (currentStep1Level <= MAX_LEVELS_STEP1) {
                         displayedWordOnMatrix[currentStep1Level - 1] = TARGET_WORD[currentStep1Level - 1];
                         displayedWordOnMatrix[currentStep1Level] = '\0'; 
                         updateMatrixDisplay(displayedWordOnMatrix);
                    }
                    spotActive = false; 
                    if (currentStep1Level >= MAX_LEVELS_STEP1) {
                        Serial.println("Adim 1'in tum seviyeleri tamamlandi!");
                        currentGameState = STATE_STEP1_ENDING_SOUND; stateEntryTime = millis();
                    } else {
                        nextSpotTime = millis() + DELAY_AFTER_CATCH_MS + random(500,1000); 
                        Serial.print(currentStep1Level); Serial.println(". seviye bitti. Sonraki spot bekleniyor...");
                    }
                } else if (millis() - spotAppearTime > SPOT_DURATION_MS) { 
                    Serial.println("Spot KACIRILDI! (Sure doldu)");
                    clearStripToBackground(); FastLED.show();
                    spotActive = false;
                    nextSpotTime = millis() + DELAY_AFTER_MISS_MS + random(500,1000); 
                    Serial.println("Ayni seviye icin yeni spot bekleniyor...");
                }
            }
            break;
        case STATE_STEP1_ENDING_SOUND:
            Serial.println("Adim 1 Bitis Sesi (Track " + String(SOUND_STEP1_END) + ") caliniyor...");
            if (dfPlayerOnline) { myDFPlayer.play(SOUND_STEP1_END); currentTrack = SOUND_STEP1_END; }
            else Serial.println("DFPlayer offline, ses calinamiyor.");
            fill_solid(leds, NUM_LEDS_STRIP, CRGB::Black); FastLED.show();
            currentGameState = STATE_STEP1_COMPLETED; stateEntryTime = millis();
            Serial.println("Oyun durumu: Seviye Bitis Sesi -> Adim 1 Tamamlandi");
            break;
        case STATE_STEP1_COMPLETED:
            if (millis() - stateEntryTime > 5000) { 
                 Serial.println("Adim 1 Tamamlandi. Sistem IDLE durumuna donuyor.");
                 updateMatrixDisplay(""); 
                 currentGameState = STATE_IDLE; stateEntryTime = millis();
                 Serial.println("Yeni oyun icin butona basin.");
            }
            break;
        default: currentGameState = STATE_IDLE; break;
    }

    if (P.displayAnimate()) { /* P.displayReset(); */ }
    // FastLED.show(); // Genellikle loop sonunda tek bir show yeterli, ama bazı efektler için içeride çağrılıyor.
                     // Eğer LED'lerde sorun olursa buraya eklenebilir.
    delay(10); 
}

void updateMatrixDisplay(const char* textToDisplay) { 
    P.displayScroll(textToDisplay, PA_CENTER, PA_SCROLL_LEFT, MATRIX_TEXT_SCROLL_SPEED); 
    Serial.print("Matrix guncellendi: "); Serial.println(textToDisplay);
}
void setStripBackground() { fill_solid(leds, NUM_LEDS_STRIP, STRIP_BG_COLOR); }
void showSpotOnStrip(int pos) { 
    setStripBackground(); 
    if (pos >= 0 && pos < NUM_LEDS_STRIP) { leds[pos] = SPOT_COLOR; }
}
void clearStripToBackground() { setStripBackground(); }

void mqtt_callback(char* topic, byte* payload, unsigned int length) { 
    Serial.println("----------------------------------------");
    Serial.print("MQTT Mesaji Geldi! Topic: ["); Serial.print(topic); Serial.println("]");
    String messageTemp;
    Serial.print("Gelen Payload (Raw String): '");
    for (unsigned int i = 0; i < length; i++) { messageTemp += (char)payload[i]; }
    Serial.print(messageTemp); Serial.println("'");
    if (String(topic) == mqtt_topic_subscribe) {
        if (messageTemp.startsWith("{\"command\":\"start_step1\"")) { 
            Serial.println("==> 'start_step1' komutu (MQTT) alindi. Oyunun mevcut durumu: " + String(currentGameState));
             if (currentGameState == STATE_IDLE) {
                currentGameState = STATE_STEP1_INITIALIZING;
                stateEntryTime = millis();
                Serial.println("Adim 1 MQTT ile baslatiliyor...");
             }
        } else if (messageTemp.startsWith("{\"command\":\"reset_esp_step1\"")) {
            Serial.println("==> 'reset_esp_step1' komutu (MQTT) alindi.");
            if (dfPlayerOnline) myDFPlayer.stop();
            currentGameState = STATE_IDLE;
            stateEntryTime = millis();
            fill_solid(leds, NUM_LEDS_STRIP, CRGB::Black); FastLED.show();
            strcpy(displayedWordOnMatrix, ""); updateMatrixDisplay("");
            currentStep1Level = 0; spotActive = false;
            Serial.println("Oyun durumu IDLE olarak ayarlandi (MQTT ile reset).");
        } else if (messageTemp == "PING") {
            Serial.println("PING alindi (MQTT), PONG gonderiliyor...");
            if(client.connected()) client.publish(mqtt_topic_publish, "PONG_from_ESP32 (Adim1 Web DZT)");
        }
    }
    Serial.println("----------------------------------------");
}
void setup_wifi() { 
    delay(10); Serial.println();
    Serial.print("WiFi'ye baglaniliyor: "); Serial.println(ssid);
    WiFi.mode(WIFI_STA); WiFi.begin(ssid, password);
    int deneme_sayaci = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500); Serial.print(".");
        if (++deneme_sayaci > 20) {
            Serial.println("\nWiFi baglantisi kurulamadi (setup)."); return;
        }
    }
    Serial.println("\nWiFi baglandi!");
    Serial.print("IP Adresi: "); Serial.println(WiFi.localIP());
}
void reconnect_mqtt() {
    if (!client.connected()) {
        Serial.print("MQTT baglantisi deneniyor...");
        String currentClientId = esp32_client_id + String(random(0xffff), HEX);
        Serial.print(" Client ID: "); Serial.println(currentClientId);
        if (client.connect(currentClientId.c_str())) {
            Serial.println("MQTT baglandi!");
            if(client.connected()) client.publish(mqtt_topic_publish, "ESP32 Online (Web DZT)");
            client.subscribe(mqtt_topic_subscribe);
            Serial.print("Abone olundu: "); Serial.println(mqtt_topic_subscribe);
        } else {
            Serial.print("MQTT baglanti HATASI, rc="); Serial.print(client.state());
            Serial.println(" - (loop icinde tekrar denenecek)");
        }
    }
}