// --- KÜTÜPHANELER ---
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

// --- DONANIM PIN TANIMLAMALARI ---
#define LED_PIN                 19
#define MATRIX_CLK_PIN          33
#define MATRIX_DATA_PIN         14
#define MATRIX_CS_PIN           27
#define BUTTON_1                32
#define BUTTON_2                4
#define BUTTON_3                12
#define BUTTON_4                13
#define DFPLAYER_RX_PIN         16
#define DFPLAYER_TX_PIN         17

// --- DONANIM AYARLARI ---
#define NUM_LEDS                69
#define BRIGHTNESS              80
#define ANIMATION_DELAY         40
#define MATRIX_HARDWARE_TYPE    MD_MAX72XX::FC16_HW
#define MATRIX_MAX_DEVICES      4

// --- SES DOSYASI NUMARALARI ---
const uint8_t SOUND_S1_WELCOME   = 1; const uint8_t SOUND_S1_END       = 2;
const uint8_t SOUND_S2_READY     = 3; const uint8_t SOUND_S2_SUCCESS   = 4;
const uint8_t SOUND_S2_ERROR     = 5;

// --- OYUN AYARLARI ---
const char STEP1_TARGET_WORD[] = "DEAD";
const char* STEP2_LEVELS[] = {"OXOX", "XOOX", "XXXO", "OOOX"};

// --- DİL AYARLARI ---
enum GameLanguage { LANG_TR, LANG_EN };
GameLanguage currentGameLanguage = LANG_TR;

// --- NESNE TANIMLAMALARI ---
CRGB leds[NUM_LEDS];
MD_Parola P = MD_Parola(MATRIX_HARDWARE_TYPE, MATRIX_DATA_PIN, MATRIX_CLK_PIN, MATRIX_CS_PIN, MATRIX_MAX_DEVICES);
HardwareSerial mp3Serial(2);
DFRobotDFPlayerMini myDFPlayer;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// --- OYUN DURUMLARI ---
enum GameState {
  WAITING_TO_START, STEP1_INIT, STEP1_PLAYING, STEP1_COMPLETE,
  WAITING_FOR_STEP2, STEP2_INIT, STEP2_PLAYING, GAME_WON
};
GameState currentState = WAITING_TO_START;
const char* gameStateNames[] = {
  "Oyunu Baslat", "Adim 1 Basliyor", "Adim 1 Oynaniyor", "Adim 1 Bitti",
  "Adim 2'yi Bekliyor", "Adim 2 Basliyor", "Adim 2 Oynaniyor", "OYUN KAZANILDI"
};

// --- OYUN DEĞİŞKENLERİ ---
unsigned long stateTimer = 0; int step1Level = 0; int step2Level = 0;
bool button_s2_state[4] = {false, false, false, false};
bool dfPlayerStatus = false; int gameVolume = 5;
char matrixBuffer[20] = "START";

// --- YARDIMCI FONKSİYONLAR ---
void playSound(uint8_t track) {
  uint8_t folder = (currentGameLanguage == LANG_TR) ? 1 : 2;
  Serial.printf("--> SES: Klasor #%d, Dosya #%d caliniyor.\n", folder, track);
  myDFPlayer.playFolder(folder, track);
}
void showOnMatrix(const char* text) {
  strcpy(matrixBuffer, text); P.displayClear();
  P.displayText(matrixBuffer, PA_CENTER, 0, 0, PA_NO_EFFECT, PA_NO_EFFECT);
  Serial.printf("--> MATRIX: Ekrana '%s' yazildi.\n", text);
}
void handleStep1();

// ======================= SETUP =========================
void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(0));

  FastLED.addLeds<WS2811, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  P.begin(); P.setIntensity(4);
  
  mp3Serial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  if (myDFPlayer.begin(mp3Serial)) {
    myDFPlayer.volume(gameVolume); dfPlayerStatus = true;
    Serial.println("[OK] DFPlayer Mini online.");
  } else { Serial.println("[HATA] DFPlayer Mini bulunamadi!"); }

  pinMode(BUTTON_1, INPUT_PULLUP); pinMode(BUTTON_2, INPUT_PULLUP);
  pinMode(BUTTON_3, INPUT_PULLUP); pinMode(BUTTON_4, INPUT_PULLUP);

  WiFi.begin("Ents_Test", "12345678");
  
  void onEvent(AsyncWebSocket*, AsyncWebSocketClient*, AwsEventType, void*, uint8_t*, size_t);
  ws.onEvent(onEvent); server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ request->send_P(200, "text/html", index_html); });
  server.begin();
  
  showOnMatrix(matrixBuffer);
}

// ======================= ANA DÖNGÜ =========================
void loop() {
  P.displayAnimate();
  ws.cleanupClients();

  switch (currentState) {
    case WAITING_TO_START:
      if (digitalRead(BUTTON_1) == LOW) { playSound(SOUND_S1_WELCOME); currentState = STEP1_INIT; }
      break;
    case STEP1_INIT:
      Serial.println("\n[DURUM] Adim 1 Basliyor: Zemin yesil yapiliyor.");
      step1Level = 0;
      fill_solid(leds, NUM_LEDS, CRGB::Green); FastLED.show();
      step1NextSpotTime = millis() + random(2500, 5000);
      showOnMatrix("");
      currentState = STEP1_PLAYING;
      break;
    case STEP1_PLAYING:
      handleStep1();
      break;
    case STEP1_COMPLETE:
      Serial.println("[DURUM] Adim 1 Tamamlandi.");
      playSound(SOUND_S1_END); showOnMatrix("DEAD");
      fill_solid(leds, NUM_LEDS, CRGB::Black); FastLED.show();
      stateTimer = millis(); currentState = WAITING_FOR_STEP2;
      break;
    case WAITING_FOR_STEP2: {
      static bool promptPlayed = false;
      if (millis() - stateTimer > 4000 && !promptPlayed) {
        playSound(SOUND_S2_READY); showOnMatrix("STEP 2?"); promptPlayed = true;
      }
      if (promptPlayed && digitalRead(BUTTON_1) == LOW) {
        step2Level = 0; promptPlayed = false; currentState = STEP2_INIT;
      }
    }
      break;
    case STEP2_INIT:
      showOnMatrix(STEP2_LEVELS[step2Level]); stateTimer = millis();
      for(int i=0;i<4;i++) button_s2_state[i] = false;
      currentState=STEP2_PLAYING;
      break;
    case STEP2_PLAYING:
      if (millis() - stateTimer > 1000) {
          char display[5]="----";
          if(digitalRead(BUTTON_1)==LOW) button_s2_state[0]=true; if(digitalRead(BUTTON_2)==LOW) button_s2_state[1]=true;
          if(digitalRead(BUTTON_3)==LOW) button_s2_state[2]=true; if(digitalRead(BUTTON_4)==LOW) button_s2_state[3]=true;
          for(int i=0;i<4;i++) { if(button_s2_state[i]) display[i]='X'; }
          showOnMatrix(display);
      }
      if (millis()-stateTimer > 1000 + 5000) {
          bool correct=true;
          for(int i=0;i<4;i++) if((STEP2_LEVELS[step2Level][i]=='X' && !button_s2_state[i]) || (STEP2_LEVELS[step2Level][i]=='O' && button_s2_state[i])) { correct=false; break; }
          if(correct) {
            playSound(SOUND_S2_SUCCESS); showOnMatrix("OK!"); step2Level++;
            if(step2Level >= 4) { currentState=GAME_WON; } else { delay(2000); currentState=STEP2_INIT; }
          } else {
            playSound(SOUND_S2_ERROR); showOnMatrix("FAIL"); delay(2000); currentState=STEP2_INIT;
          }
      }
      break;
    case GAME_WON: {
      static bool finalActionDone = false;
      if (!finalActionDone) { playSound(SOUND_S2_SUCCESS); showOnMatrix("FINISH"); finalActionDone = true; }
      static unsigned long lastBlink = 0;
      if(millis() - lastBlink > 100) {
          lastBlink = millis(); static bool on = false; on = !on;
          if(on) { fill_solid(leds,NUM_LEDS,CRGB::Green); } else { fill_solid(leds,NUM_LEDS,CRGB::Black); }
          FastLED.show();
      }
    }
      break;
  }
  
  static unsigned long lastNotify = 0;
  if (millis() - lastNotify > 250) { /* notifyClients() çağrısı */ }
}

// ======================= YENİ Adım 1 OYUN MANTIĞI =========================
void handleStep1() {
  static bool spotIsActive = false;
  static int currentSpotPos = -1;

  if (!spotIsActive && millis() > step1NextSpotTime) {
      currentSpotPos = random(NUM_LEDS - 5);
      for(int i=0; i<5; i++) leds[currentSpotPos + i] = CRGB::Red;
      FastLED.show();
      Serial.printf("Adim 1: Kirmizi bolge LED %d'den baslayarak aktif.\n", currentSpotPos);
      stateTimer = millis();
      spotIsActive = true;
  }
  
  if (spotIsActive) {
      if (digitalRead(BUTTON_1) == LOW) {
        step1Level++;
        Serial.printf("Adim 1: BASARILI YAKALAMA! Seviye: %d\n", step1Level);
        String pWord; for(int i=0; i<step1Level; i++) pWord += STEP1_TARGET_WORD[i];
        showOnMatrix(pWord.c_str());
        spotIsActive = false; fill_solid(leds, NUM_LEDS, CRGB::Green); FastLED.show();
        if (step1Level >= 4) { currentState = STEP1_COMPLETE; } 
        else { step1NextSpotTime = millis() + random(2500, 4000); }
      } else if (millis() - stateTimer > 1500) {
        Serial.println("Adim 1: Sure doldu, KACIRILDI!");
        spotIsActive = false; fill_solid(leds, NUM_LEDS, CRGB::Green); FastLED.show();
        step1NextSpotTime = millis() + random(2500, 4000);
      }
  }
}

// ======================= WEB VE DİĞER FONKSİYONLAR =========================
void onEvent(AsyncWebSocket*, AsyncWebSocketClient*, AwsEventType, void*, uint8_t*, size_t) { /* ... */ }
void notifyClients() { /* ... */ }