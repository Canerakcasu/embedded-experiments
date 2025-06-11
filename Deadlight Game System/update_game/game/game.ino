
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Arduino_JSON.h>
#include <FastLED.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include "DFRobotDFPlayerMini.h"
#include "index.h"

//--- DONANIM PIN TANIMLAMALARI ---
#define LED_PIN              19
#define MATRIX_CLK_PIN       33
#define MATRIX_DATA_PIN      14
#define MATRIX_CS_PIN        27
#define BUTTON_1             32
#define BUTTON_2             4
#define BUTTON_3             12
#define BUTTON_4             13
#define DFPLAYER_RX_PIN      16
#define DFPLAYER_TX_PIN      17

//--- DONANIM ve OYUN AYARLARI ---
// 69 fiziksel LED = 23 Piksel.
#define NUM_PIXELS           23
#define BRIGHTNESS           80
#define MATRIX_HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MATRIX_MAX_DEVICES   4

// Adım 1 (Reaksiyon Oyunu) Ayarları
#define PROGRESS_COLOR       CRGB::Blue
#define ALERT_COLOR          CRGB::Red
#define SUCCESS_COLOR        CRGB::LawnGreen
#define FAIL_COLOR           CRGB::DarkOrange
#define ANIMATION_INTERVAL   250
#define REACTION_WINDOW      1500
#define RESULT_SHOW_TIME     2000
#define CHUNK_SIZE           1

//--- SES DOSYASI NUMARALARI ---
const uint8_t SOUND_S1_WELCOME   = 1;
const uint8_t SOUND_S1_END       = 2;
const uint8_t SOUND_S2_READY     = 3;
const uint8_t SOUND_S2_SUCCESS   = 4;
const uint8_t SOUND_S2_ERROR     = 5;

//--- GLOBAL DEĞİŞKENLER ve NESNELER ---

// Oyun Ayarları
const char STEP1_TARGET_WORD[] = "DEAD";
const char* STEP2_LEVELS[] = {"OXOX", "XOOX", "XXXO", "OOOX"};

// Dil Ayarları
enum GameLanguage { LANG_TR, LANG_EN };
GameLanguage currentGameLanguage = LANG_TR;

// Donanım Nesneleri
CRGB leds[NUM_PIXELS];
MD_Parola P = MD_Parola(MATRIX_HARDWARE_TYPE, MATRIX_DATA_PIN, MATRIX_CLK_PIN, MATRIX_CS_PIN, MATRIX_MAX_DEVICES);
HardwareSerial mp3Serial(2);
DFRobotDFPlayerMini myDFPlayer;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Oyun Durumları
enum GameState {
  WAITING_TO_START, STEP1_INIT, STEP1_PLAYING, STEP1_COMPLETE,
  WAITING_FOR_STEP2, STEP2_INIT, STEP2_PLAYING, GAME_WON
};
GameState currentState = WAITING_TO_START;
const char* gameStateNames[] = {
  "Oyunu Baslat", "Adim 1 Oynaniyor", "Adim 1 Oynaniyor", "Adim 1 Bitti",
  "Adim 2'yi Bekliyor", "Adim 2 Basliyor", "Adim 2 Oynaniyor", "OYUN KAZANILDI"
};

// Adım 1'in Kendi İç Durumları
enum Step1Phase { S1_ANIMATING, S1_WAITING_FOR_PRESS, S1_SHOWING_SUCCESS, S1_SHOWING_FAILURE };
Step1Phase currentStep1Phase = S1_ANIMATING;

// Genel Oyun Değişkenleri
unsigned long stateTimer = 0;
int step1Level = 0;
int step2Level = 0;
bool button_s2_state[4] = {false, false, false, false};
bool dfPlayerStatus = false;
int gameVolume = 5;
char matrixBuffer[20] = "START";

// Adım 1'e Özel Değişkenler
int ledProgress = 0;
int currentStep = 0;
int targetStep = 0;

//--- YARDIMCI FONKSİYONLAR (Prototipler) ---
void playSound(uint8_t track);
void showOnMatrix(const char* text);
void handleStep1();
void resetStep1Round();
void notifyClients();
void onEvent(AsyncWebSocket*, AsyncWebSocketClient*, AwsEventType, void*, uint8_t*, size_t);

//--- YARDIMCI FONKSİYONLAR (GÖVDELERİ) ---
void playSound(uint8_t track) {
  uint8_t folder = (currentGameLanguage == LANG_TR) ? 1 : 2;
  Serial.printf("--> SES: Klasor #%d, Dosya #%d caliniyor.\n", folder, track);
  if (dfPlayerStatus) myDFPlayer.playFolder(folder, track);
}
void showOnMatrix(const char* text) {
  strcpy(matrixBuffer, text);
  P.displayClear();
  P.displayText(matrixBuffer, PA_CENTER, 0, 0, PA_NO_EFFECT, PA_NO_EFFECT);
  Serial.printf("--> MATRIX: Ekrana '%s' yazildi.\n", text);
}

// ======================= SETUP FONKSİYONU =========================
void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(0));

  FastLED.addLeds<WS2811, LED_PIN, GRB>(leds, NUM_PIXELS);
  FastLED.setBrightness(BRIGHTNESS);
  fill_solid(leds, NUM_PIXELS, CRGB::Black);
  FastLED.show();

  P.begin();
  P.setIntensity(4);

  mp3Serial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  if (myDFPlayer.begin(mp3Serial)) {
    myDFPlayer.volume(gameVolume);
    dfPlayerStatus = true;
    Serial.println("[OK] DFPlayer Mini online.");
  } else {
    Serial.println("[HATA] DFPlayer Mini bulunamadi!");
  }

  pinMode(BUTTON_1, INPUT_PULLUP);
  pinMode(BUTTON_2, INPUT_PULLUP);
  pinMode(BUTTON_3, INPUT_PULLUP);
  pinMode(BUTTON_4, INPUT_PULLUP);

  Serial.println("\nWiFi agina baglaniliyor...");
  WiFi.begin("Ents_Test", "12345678");
  int wifi_retries = 0;
  while (WiFi.status() != WL_CONNECTED && wifi_retries < 30) {
    delay(500);
    Serial.print(".");
    wifi_retries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi baglantisi basarili!");
    Serial.print("IP Adresi: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi baglantisi basarisiz! WiFi olmadan devam ediliyor...");
  }
  
  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ request->send_P(200, "text/html", index_html); });
  server.begin();
  
  showOnMatrix(matrixBuffer);
}

// ======================= ANA DÖNGÜ (LOOP) =========================
void loop() {
  if (P.displayAnimate()) {
    P.displayReset();
  }
  ws.cleanupClients();

  switch (currentState) {
    case WAITING_TO_START:
      if (digitalRead(BUTTON_1) == LOW) { playSound(SOUND_S1_WELCOME); currentState = STEP1_INIT; delay(200); }
      break;

    case STEP1_INIT:
      Serial.println("\n[DURUM] Adim 1 Basliyor: Reaksiyon oyunu kuruluyor.");
      step1Level = 0;
      showOnMatrix("");
      resetStep1Round();
      currentState = STEP1_PLAYING;
      break;

    case STEP1_PLAYING:
      handleStep1();
      break;

    case STEP1_COMPLETE:
      Serial.println("[DURUM] Adim 1 Tamamlandi.");
      playSound(SOUND_S1_END);
      showOnMatrix("DEAD");
      fill_solid(leds, NUM_PIXELS, CRGB::Black);
      FastLED.show();
      stateTimer = millis();
      currentState = WAITING_FOR_STEP2;
      break;

    case WAITING_FOR_STEP2: {
      static bool promptPlayed = false;
      if (millis() - stateTimer > 4000 && !promptPlayed) {
        playSound(SOUND_S2_READY); showOnMatrix("STEP 2?"); promptPlayed = true;
      }
      if (promptPlayed && digitalRead(BUTTON_1) == LOW) {
        step2Level = 0; promptPlayed = false; currentState = STEP2_INIT; delay(200);
      }
    }
      break;

    case STEP2_INIT:
      showOnMatrix(STEP2_LEVELS[step2Level]);
      stateTimer = millis();
      for(int i=0; i<4; i++) button_s2_state[i] = false;
      currentState = STEP2_PLAYING;
      break;

    case STEP2_PLAYING: {
      static char lastDisplayS2[5] = "";
      if (millis() - stateTimer > 1000) {
        char display[5] = "----";
        if (digitalRead(BUTTON_1) == LOW) button_s2_state[0] = true;
        if (digitalRead(BUTTON_2) == LOW) button_s2_state[1] = true;
        if (digitalRead(BUTTON_3) == LOW) button_s2_state[2] = true;
        if (digitalRead(BUTTON_4) == LOW) button_s2_state[3] = true;
        for (int i = 0; i < 4; i++) { if (button_s2_state[i]) display[i] = 'X'; }
        
        if (strcmp(display, lastDisplayS2) != 0) {
          showOnMatrix(display);
          strcpy(lastDisplayS2, display);
        }
      }
      if (millis() - stateTimer > 1000 + 5000) {
        bool correct = true;
        for (int i = 0; i < 4; i++) {
          if ((STEP2_LEVELS[step2Level][i] == 'X' && !button_s2_state[i]) ||
              (STEP2_LEVELS[step2Level][i] == 'O' && button_s2_state[i])) {
            correct = false;
            break;
          }
        }
        if (correct) {
          playSound(SOUND_S2_SUCCESS); showOnMatrix("OK!"); step2Level++;
          if (step2Level >= 4) { currentState = GAME_WON; } 
          else { delay(2000); currentState = STEP2_INIT; }
        } else {
          playSound(SOUND_S2_ERROR); showOnMatrix("FAIL"); delay(2000);
          currentState = STEP2_INIT;
        }
        strcpy(lastDisplayS2, "");
      }
    }
      break;

    case GAME_WON: {
      static bool finalActionDone = false;
      if (!finalActionDone) { playSound(SOUND_S2_SUCCESS); showOnMatrix("FINISH"); finalActionDone = true; }
      static unsigned long lastBlink = 0;
      if(millis() - lastBlink > 100) {
        lastBlink = millis(); static bool on = false; on = !on;
        if(on) { fill_solid(leds, NUM_PIXELS, CRGB::Green); } else { fill_solid(leds, NUM_PIXELS, CRGB::Black); }
        FastLED.show();
      }
    }
      break;
  }

  static unsigned long lastNotify = 0;
  if (millis() - lastNotify > 500) {
   lastNotify = millis();
   notifyClients();
  }
}

// ======================= OYUN MANTIK FONKSİYONLARI =========================

void handleStep1() {
  switch (currentStep1Phase) {
    case S1_ANIMATING:
      if (digitalRead(BUTTON_1) == LOW) {
        Serial.println("HATA: Erken basıldı!");
        playSound(SOUND_S2_ERROR);
        fill_solid(leds, NUM_PIXELS, FAIL_COLOR);
        FastLED.show();
        showOnMatrix("FAIL!");
        stateTimer = millis();
        currentStep1Phase = S1_SHOWING_FAILURE;
        delay(200);
        return;
      }

      if (millis() - stateTimer > ANIMATION_INTERVAL) {
        stateTimer = millis();
        ledProgress += CHUNK_SIZE;
        currentStep++;

        if (ledProgress >= NUM_PIXELS) {
          ledProgress = 0;
          currentStep = 0; 
        }
        
        if (currentStep == targetStep) {
          fill_solid(leds, NUM_PIXELS, CRGB::Black);
          for (int i = 0; i < CHUNK_SIZE; i++) {
            if (ledProgress + i < NUM_PIXELS) leds[ledProgress + i] = ALERT_COLOR;
          }
          FastLED.show();
          stateTimer = millis();
          currentStep1Phase = S1_WAITING_FOR_PRESS;
          Serial.println("--> KIRMIZI YANDI! BEKLENİYOR...");
          return;
        }

        fill_solid(leds, NUM_PIXELS, CRGB::Black);
        for (int i = 0; i < CHUNK_SIZE; i++) {
          if (ledProgress + i < NUM_PIXELS) leds[ledProgress + i] = PROGRESS_COLOR;
        }
        FastLED.show();
      }
      break;

    case S1_WAITING_FOR_PRESS:
      if (digitalRead(BUTTON_1) == LOW) {
        step1Level++;
        playSound(SOUND_S2_SUCCESS);
        fill_solid(leds, NUM_PIXELS, SUCCESS_COLOR);
        FastLED.show();
        String pWord; 
        for(int i=0; i<step1Level; i++) pWord += STEP1_TARGET_WORD[i];
        showOnMatrix(pWord.c_str());
        stateTimer = millis();
        currentStep1Phase = S1_SHOWING_SUCCESS;
      } 
      else if (millis() - stateTimer > REACTION_WINDOW) {
        playSound(SOUND_S2_ERROR);
        fill_solid(leds, NUM_PIXELS, FAIL_COLOR);
        FastLED.show();
        showOnMatrix("FAIL!");
        stateTimer = millis();
        currentStep1Phase = S1_SHOWING_FAILURE;
      }
      break;

    case S1_SHOWING_SUCCESS:
      if (millis() - stateTimer > RESULT_SHOW_TIME) {
        if (step1Level >= 4) {
          currentState = STEP1_COMPLETE;
        } else {
          resetStep1Round();
        }
      }
      break;
      
    case S1_SHOWING_FAILURE:
      if (millis() - stateTimer > RESULT_SHOW_TIME) {
        showOnMatrix("START");
        fill_solid(leds, NUM_PIXELS, CRGB::Black);
        FastLED.show();
        currentState = WAITING_TO_START;
      }
      break;
  }
}

void resetStep1Round() {
  ledProgress = -CHUNK_SIZE;
  currentStep = 0;

  int totalStepsInLoop = NUM_PIXELS / CHUNK_SIZE;
  targetStep = random(4, totalStepsInLoop - 2); 
  
  Serial.printf("YENİ TUR: Toplam Adim: %d, Hedef Adim: %d\n", totalStepsInLoop, targetStep);
  
  fill_solid(leds, NUM_PIXELS, CRGB::Black);
  FastLED.show();
  currentStep1Phase = S1_ANIMATING;
  stateTimer = millis();
}

// ======================= WEB SUNUCU FONKSİYONLARI =========================
void notifyClients() {
  static JSONVar gameStateJson; 

  gameStateJson["gameState"] = gameStateNames[currentState];
  gameStateJson["step1Level"] = step1Level;
  gameStateJson["step2Level"] = step2Level;
  gameStateJson["matrix"] = String(matrixBuffer);
  gameStateJson["volume"] = gameVolume;
  gameStateJson["language"] = (currentGameLanguage == LANG_TR) ? "TR" : "EN";

  String jsonString = JSON.stringify(gameStateJson);
  ws.textAll(jsonString);
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      notifyClients();
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      if (len == 0) return;
      {
        JSONVar jsonData = JSON.parse((char*)data);
        if (JSON.typeof(jsonData) == "undefined") {
          Serial.println("WebSocket Error: JSON parsing failed");
          return;
        }

        if (jsonData.hasOwnProperty("command")) {
          String command = (const char*)jsonData["command"];
          Serial.printf("--> WebSocket Komutu Alındı: %s\n", command.c_str());

          if (command == "startGame" && currentState == WAITING_TO_START) {
            playSound(SOUND_S1_WELCOME);
            currentState = STEP1_INIT;
          }
          else if (command == "setVolume") {
            gameVolume = (int)jsonData["value"];
            if (gameVolume < 0) gameVolume = 0;
            if (gameVolume > 30) gameVolume = 30;
            myDFPlayer.volume(gameVolume);
          }
          else if (command == "setLanguage") {
            String lang = (const char*)jsonData["value"];
            currentGameLanguage = (lang == "TR") ? LANG_TR : LANG_EN;
            myDFPlayer.stop();
          }
          
          notifyClients();
        }
      }
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}