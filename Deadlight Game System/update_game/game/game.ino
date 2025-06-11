/**
 * @file game.ino
 * @brief Deadlight Game System - Interactive Escape Room Prop with WebSocket and MQTT control.
 * @version 4.0 - Final Polished
 * @date 11 June 2025
 */

//--- LIBRARIES ---
#include <WiFi.h>
#include <PubSubClient.h> 
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Arduino_JSON.h>
#include <FastLED.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include "DFRobotDFPlayerMini.h"
#include "index.h"

//--- NETWORK & MQTT CONFIGURATION ---
const char* wifi_ssid = "Ents_Test";
const char* wifi_password = "12345678";
const char* mqtt_server = "192.168.20.208"; // DEĞİŞTİR: Kendi MQTT Sunucu IP Adresiniz
const int   mqtt_port = 1883;
const char* command_topic = "game/control";
const char* status_topic = "game/status";

//--- HARDWARE & GAME DEFINITIONS ---
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

#define NUM_PIXELS           23
#define BRIGHTNESS           80
#define MATRIX_HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MATRIX_MAX_DEVICES   4

// Step 1 Game Settings
#define PROGRESS_COLOR       CRGB::Green
#define ALERT_COLOR          CRGB::Red
#define SUCCESS_COLOR        CRGB::LawnGreen
#define FAIL_COLOR           CRGB::DarkOrange
#define BASE_ANIMATION_INTERVAL   250
#define BASE_REACTION_WINDOW      500
#define RESULT_SHOW_TIME     2000
#define CHUNK_SIZE           1

// Sound File Numbers
const uint8_t SOUND_S1_WELCOME = 1, SOUND_S1_END = 2, SOUND_S2_READY = 3, SOUND_S2_SUCCESS = 4, SOUND_S2_ERROR = 5, SOUND_FINISH = 6;

//--- GLOBAL OBJECTS & VARIABLES ---
// Game Content
const char STEP1_TARGET_WORD[] = "DEAD";
const char* STEP2_LEVELS[] = {"OXOX", "XOOX", "XXXO", "OOOX"};

// Enums for State Management
enum GameLanguage { LANG_TR, LANG_EN };
enum GameState { WAITING_TO_START, STEP1_INIT, STEP1_PLAYING, STEP1_COMPLETE, WAITING_FOR_STEP2, STEP2_INIT, STEP2_PLAYING, GAME_WON };
enum Step1Phase { S1_PLAYING, S1_SHOWING_SUCCESS, S1_SHOWING_FAILURE };

// This array must be defined AFTER the GameState enum.
const char* gameStateNames[] = {
  "Oyunu Baslat", "Adim 1 Oynaniyor", "Adim 1 Oynaniyor", "Adim 1 Bitti",
  "Adim 2'yi Bekliyor", "Adim 2 Basliyor", "Adim 2 Oynaniyor", "OYUN KAZANILDI"
};

// Hardware & Server Objects
CRGB leds[NUM_PIXELS];
MD_Parola P(MATRIX_HARDWARE_TYPE, MATRIX_DATA_PIN, MATRIX_CLK_PIN, MATRIX_CS_PIN, MATRIX_MAX_DEVICES);
HardwareSerial mp3Serial(2);
DFRobotDFPlayerMini myDFPlayer;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// State Variables
GameState currentState = WAITING_TO_START;
Step1Phase currentStep1Phase = S1_PLAYING;
GameLanguage currentGameLanguage = LANG_TR;
unsigned long stateTimer = 0;
int step1Level = 0, step2Level = 0;
bool button_s2_state[4] = {false, false, false, false};
bool dfPlayerStatus = false, wifiStatus = false;
int gameVolume = 5;
char matrixBuffer[20] = "START";
int ledProgress = 0, currentStep = 0, targetStep = 0;
bool alertIsActive = false;
unsigned long alertTimer = 0;
int currentAnimationInterval = BASE_ANIMATION_INTERVAL;
int currentReactionWindow = BASE_REACTION_WINDOW;

//--- FUNCTION PROTOTYPES ---
void playSound(uint8_t);
void showOnMatrix(const char*);
void handleStep1();
void resetStep1Round();
String getStatusJSON();
void notifyClients();
void publishStatus();
void processAction(JSONVar&);
void mqttCallback(char*, byte*, unsigned int);
void reconnectMQTT();
void onEvent(AsyncWebSocket*, AsyncWebSocketClient*, AwsEventType, void*, uint8_t*, size_t);

// ======================= SETUP =========================
void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(0));

  FastLED.addLeds<WS2811, LED_PIN, BRG>(leds, NUM_PIXELS);
  FastLED.setBrightness(BRIGHTNESS);
  fill_solid(leds, NUM_PIXELS, CRGB::Black);
  FastLED.show();
  
  P.begin();
  P.setIntensity(4);

  mp3Serial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  if (myDFPlayer.begin(mp3Serial)) { myDFPlayer.volume(gameVolume); dfPlayerStatus = true; }
  
  pinMode(BUTTON_1, INPUT_PULLUP); pinMode(BUTTON_2, INPUT_PULLUP);
  pinMode(BUTTON_3, INPUT_PULLUP); pinMode(BUTTON_4, INPUT_PULLUP);

  WiFi.begin(wifi_ssid, wifi_password);
  int wifi_retries = 0;
  while (WiFi.status() != WL_CONNECTED && wifi_retries < 20) { delay(500); wifi_retries++; }
  wifiStatus = (WiFi.status() == WL_CONNECTED);
  
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);
  
  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ request->send_P(200, "text/html", index_html); });
  server.begin();
  
  showOnMatrix("READY");
}

// ======================= MAIN LOOP =========================
void loop() {
  if (P.displayAnimate()) P.displayReset();
  ws.cleanupClients();
  
  if (wifiStatus && !mqttClient.connected()) reconnectMQTT();
  mqttClient.loop();

  switch (currentState) {
    case WAITING_TO_START:
      if (digitalRead(BUTTON_1) == LOW) { playSound(SOUND_S1_WELCOME); currentState = STEP1_INIT; delay(200); }
      break;
    case STEP1_INIT:
      step1Level = 0; showOnMatrix(""); resetStep1Round(); currentState = STEP1_PLAYING;
      break;
    case STEP1_PLAYING:
      handleStep1();
      break;
    case STEP1_COMPLETE:
      playSound(SOUND_S1_END); showOnMatrix("DEAD"); fill_solid(leds, NUM_PIXELS, CRGB::Black); FastLED.show();
      stateTimer = millis(); currentState = WAITING_FOR_STEP2;
      break;
    case WAITING_FOR_STEP2: {
      static bool promptPlayed = false;
      if (millis() - stateTimer > 4000 && !promptPlayed) { playSound(SOUND_S2_READY); showOnMatrix("STEP 2?"); promptPlayed = true; }
      if (promptPlayed && digitalRead(BUTTON_1) == LOW) { step2Level = 0; promptPlayed = false; currentState = STEP2_INIT; delay(200); }
    } break;
    case STEP2_INIT:
      showOnMatrix(STEP2_LEVELS[step2Level]); stateTimer = millis();
      for(int i=0; i<4; i++) button_s2_state[i] = false;
      currentState = STEP2_PLAYING;
      break;
    case STEP2_PLAYING: {
      static char lastDisplayS2[5] = "";
      if (millis() - stateTimer > 1000) {
        char display[5] = "----";
        if (digitalRead(BUTTON_1) == LOW) button_s2_state[0] = true; if (digitalRead(BUTTON_2) == LOW) button_s2_state[1] = true;
        if (digitalRead(BUTTON_3) == LOW) button_s2_state[2] = true; if (digitalRead(BUTTON_4) == LOW) button_s2_state[3] = true;
        for (int i=0; i<4; i++) { if (button_s2_state[i]) display[i] = 'X'; }
        if (strcmp(display, lastDisplayS2) != 0) { showOnMatrix(display); strcpy(lastDisplayS2, display); }
      }
      if (millis() - stateTimer > 6000) {
        bool correct = true;
        for (int i=0; i<4; i++) {
          if ((STEP2_LEVELS[step2Level][i] == 'X' && !button_s2_state[i]) || (STEP2_LEVELS[step2Level][i] == 'O' && button_s2_state[i])) {
            correct = false; break;
          }
        }
        if (correct) {
          playSound(SOUND_S2_SUCCESS); showOnMatrix("OK!"); step2Level++;
          if (step2Level >= 4) { currentState = GAME_WON; } else { delay(2000); currentState = STEP2_INIT; }
        } else {
          playSound(SOUND_S2_ERROR); showOnMatrix("FAIL"); delay(2000); currentState = STEP2_INIT;
        }
        strcpy(lastDisplayS2, "");
      }
    } break;
    case GAME_WON: {
      static bool finalActionDone = false;
      if (!finalActionDone) { playSound(SOUND_FINISH); showOnMatrix("FINISH"); finalActionDone = true; }
      static unsigned long lastBlink = 0;
      if(millis() - lastBlink > 100) {
        lastBlink = millis(); static bool on = false; on = !on;
        if(on) fill_solid(leds, NUM_PIXELS, CRGB::Green); else fill_solid(leds, NUM_PIXELS, CRGB::Black);
        FastLED.show();
      }
    } break;
  }
  
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 1000) {
   lastUpdate = millis();
   notifyClients();
   publishStatus();
  }
}

//--- HELPER FUNCTIONS (DEFINITIONS) ---
void playSound(uint8_t track) {
  uint8_t folder = (currentGameLanguage == LANG_TR) ? 1 : 2;
  if (dfPlayerStatus) myDFPlayer.playFolder(folder, track);
}
void showOnMatrix(const char* text) {
  strcpy(matrixBuffer, text);
  P.displayClear();
  P.displayText(matrixBuffer, PA_CENTER, 0, 0, PA_NO_EFFECT, PA_NO_EFFECT);
}

// ======================= GAME LOGIC FUNCTIONS =========================
void handleStep1() {
  switch (currentStep1Phase) {
    case S1_PLAYING:
      if (digitalRead(BUTTON_1) == LOW) {
        if (alertIsActive && (millis() - alertTimer < currentReactionWindow)) {
          step1Level++; playSound(SOUND_S2_SUCCESS); fill_solid(leds, NUM_PIXELS, SUCCESS_COLOR); FastLED.show();
          String pWord; for(int i=0; i<step1Level; i++) pWord += STEP1_TARGET_WORD[i];
          showOnMatrix(pWord.c_str());
          stateTimer = millis(); currentStep1Phase = S1_SHOWING_SUCCESS;
        } else {
          playSound(SOUND_S2_ERROR); fill_solid(leds, NUM_PIXELS, FAIL_COLOR); FastLED.show();
          showOnMatrix("FAIL!"); stateTimer = millis(); currentStep1Phase = S1_SHOWING_FAILURE;
        }
        delay(200); return;
      }
      if (millis() - stateTimer > currentAnimationInterval) {
        stateTimer = millis(); ledProgress += CHUNK_SIZE; currentStep++;
        if (ledProgress >= NUM_PIXELS) { ledProgress = 0; currentStep = 0; }
        if (alertIsActive && (millis() - alertTimer >= currentReactionWindow)) { alertIsActive = false; }
        fill_solid(leds, NUM_PIXELS, CRGB::Black);
        CRGB blockColor = PROGRESS_COLOR;
        if (currentStep == targetStep && !alertIsActive) { alertIsActive = true; alertTimer = millis(); }
        if (alertIsActive) { blockColor = ALERT_COLOR; }
        for (int i = 0; i < CHUNK_SIZE; i++) { if (ledProgress + i < NUM_PIXELS) leds[ledProgress + i] = blockColor; }
        FastLED.show();
      }
      break;
    case S1_SHOWING_SUCCESS:
      if (millis() - stateTimer > RESULT_SHOW_TIME) {
        alertIsActive = false;
        if (step1Level >= 4) { currentState = STEP1_COMPLETE; } else { resetStep1Round(); }
      }
      break;
    case S1_SHOWING_FAILURE:
      if (millis() - stateTimer > RESULT_SHOW_TIME) {
        alertIsActive = false; showOnMatrix("START"); fill_solid(leds, NUM_PIXELS, CRGB::Black); FastLED.show();
        currentState = WAITING_TO_START;
      }
      break;
  }
}
void resetStep1Round() {
  currentAnimationInterval = BASE_ANIMATION_INTERVAL - (step1Level * 35);
  currentReactionWindow = BASE_REACTION_WINDOW - (step1Level * 60);
  if (currentAnimationInterval < 100) currentAnimationInterval = 100;
  if (currentReactionWindow < 250) currentReactionWindow = 250;
  
  ledProgress = -CHUNK_SIZE; currentStep = 0; alertIsActive = false;
  int totalStepsInLoop = NUM_PIXELS / CHUNK_SIZE;
  // --- DEĞİŞİKLİK: Kırmızı ışığın daha geç çıkması için alt limit artırıldı. ---
  targetStep = random(8, totalStepsInLoop - 2); 
  
  fill_solid(leds, NUM_PIXELS, CRGB::Black);
  FastLED.show();
  currentStep1Phase = S1_PLAYING;
  stateTimer = millis();
}

// ======================= COMMUNICATION FUNCTIONS =========================
String getStatusJSON() {
  static JSONVar gameStateJson; 
  gameStateJson["gameState"] = gameStateNames[currentState];
  char levelDetail[32];
  if (currentState == STEP1_PLAYING || currentState == STEP1_INIT) { sprintf(levelDetail, "Step 1 - Letter %d / 4", step1Level + 1); }
  else if (currentState == STEP2_PLAYING || currentState == STEP2_INIT) { sprintf(levelDetail, "Step 2 - Level %d / 4", step2Level + 1); }
  else { strcpy(levelDetail, "--"); }
  gameStateJson["levelDetail"] = levelDetail;
  gameStateJson["matrix"] = String(matrixBuffer);
  gameStateJson["volume"] = gameVolume;
  gameStateJson["language"] = (currentGameLanguage == LANG_TR) ? "TR" : "EN";
  gameStateJson["dfPlayerStatus"] = dfPlayerStatus;
  gameStateJson["wifiStatus"] = wifiStatus;
  return JSON.stringify(gameStateJson);
}
void processAction(JSONVar& jsonData) {
  if (jsonData.hasOwnProperty("action")) {
    String action = (const char*)jsonData["action"];
    if (action == "start_game" && currentState == WAITING_TO_START) { playSound(SOUND_S1_WELCOME); currentState = STEP1_INIT; }
    else if (action == "reset_game") { showOnMatrix("RESET"); delay(1000); ESP.restart(); }
    else if (action == "set_volume") {
      gameVolume = (int)jsonData["value"];
      if (gameVolume < 0) gameVolume = 0; if (gameVolume > 30) gameVolume = 30;
      myDFPlayer.volume(gameVolume);
    }
    else if (action == "set_language") {
      String lang = (const char*)jsonData["value"];
      currentGameLanguage = (lang == "TR") ? LANG_TR : LANG_EN;
      myDFPlayer.stop();
    }
  }
}
void notifyClients() { ws.textAll(getStatusJSON()); }
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if(type == WS_EVT_DATA){
    JSONVar jsonData = JSON.parse((char*)data);
    processAction(jsonData);
    notifyClients();
  }
}
void publishStatus() {
  if (mqttClient.connected()) { mqttClient.publish(status_topic, getStatusJSON().c_str()); }
}
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message; for (int i = 0; i < length; i++) message += (char)payload[i];
  JSONVar jsonData = JSON.parse(message);
  processAction(jsonData);
  publishStatus();
}
void reconnectMQTT() {
  while (!mqttClient.connected()) {
    String clientId = "ESP32_GameClient-" + String(random(0xffff), HEX);
    if (mqttClient.connect(clientId.c_str())) {
      mqttClient.subscribe(command_topic);
    } else {
      delay(5000);
    }
  }
}