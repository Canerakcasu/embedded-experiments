/**
 * @file game.ino
 * @brief Deadlight Game System - Final version with all bug fixes and features.
 * @version 6.0 - Final
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
const char* mqtt_server = "192.168.20.208"; 
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

// Game Timer
const unsigned long GAME_DURATION = 10 * 60 * 1000;

// Sound File Numbers
const uint8_t SOUND_S1_WELCOME = 1, SOUND_S1_END = 2, SOUND_S2_READY = 3, SOUND_S2_SUCCESS = 4, SOUND_S2_ERROR = 5, SOUND_FINISH = 6;

//--- GLOBAL OBJECTS & VARIABLES ---
const char STEP1_TARGET_WORD[] = "DEAD";
const char* STEP2_LEVELS[] = {"OXOX", "XOOX", "XXXO", "OOOX"};
enum GameLanguage { LANG_TR, LANG_EN };
enum GameState { WAITING_TO_START, STEP1_INIT, STEP1_PLAYING, STEP1_COMPLETE, WAITING_FOR_STEP2, STEP2_INIT, STEP2_PLAYING, GAME_WON, GAME_OVER_TIMEOUT };
const char* gameStateNames[] = {
  "Oyunu Baslat", "Adim 1 Oynaniyor", "Adim 1 Oynaniyor", "Adim 1 Tamamlandi",
  "Adim 2'yi Bekliyor", "Adim 2 Basliyor", "Adim 2 Oynaniyor", "OYUN KAZANILDI", "SURE DOLDU"
};

CRGB leds[NUM_PIXELS];
MD_Parola P(MATRIX_HARDWARE_TYPE, MATRIX_DATA_PIN, MATRIX_CLK_PIN, MATRIX_CS_PIN, MATRIX_MAX_DEVICES);
HardwareSerial mp3Serial(2);
DFRobotDFPlayerMini myDFPlayer;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
WiFiClient espClient;
PubSubClient mqttClient(espClient);

GameState currentState = WAITING_TO_START;
GameLanguage soundLanguage = LANG_TR;
unsigned long stateTimer = 0;
int step1Level = 0;
int step2Level = 0;
bool button_s2_state[4] = {false, false, false, false};
bool dfPlayerStatus = false, wifiStatus = false;
int gameVolume = 5;
char matrixBuffer[20] = "START";
int ledProgress = 0;
int targetStep = 0;
bool alertIsActive = false;
unsigned long alertTimer = 0;
int currentAnimationInterval = BASE_ANIMATION_INTERVAL;
int currentReactionWindow = BASE_REACTION_WINDOW;
unsigned long gameStartTime = 0;
bool gameTimerIsActive = false;

//--- FUNCTION PROTOTYPES ---
void playSound(uint8_t); void showOnMatrix(const char*); void handleStep1(); void resetStep1Round(); String getStatusJSON();
void notifyClients(); void publishStatus(); void processAction(JSONVar&); void mqttCallback(char*, byte*, unsigned int);
void reconnectMQTT();

// ======================= SETUP =========================
void setup() {
  Serial.begin(115200); randomSeed(analogRead(0));
  FastLED.addLeds<WS2811, LED_PIN, BRG>(leds, NUM_PIXELS); FastLED.setBrightness(BRIGHTNESS);
  fill_solid(leds, NUM_PIXELS, CRGB::Black); FastLED.show();
  P.begin(); P.setIntensity(4);
  mp3Serial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  if (myDFPlayer.begin(mp3Serial)) { myDFPlayer.volume(gameVolume); dfPlayerStatus = true; }
  pinMode(BUTTON_1, INPUT_PULLUP); pinMode(BUTTON_2, INPUT_PULLUP); pinMode(BUTTON_3, INPUT_PULLUP); pinMode(BUTTON_4, INPUT_PULLUP);
  WiFi.begin(wifi_ssid, wifi_password); int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) { delay(500); retries++; }
  wifiStatus = (WiFi.status() == WL_CONNECTED);
  mqttClient.setServer(mqtt_server, mqtt_port); mqttClient.setCallback(mqttCallback);
  ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_DATA) {
      JSONVar jsonData = JSON.parse((char*)data);
      processAction(jsonData);
      notifyClients();
    }
  });
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

  if (gameTimerIsActive && (millis() - gameStartTime > GAME_DURATION)) {
    playSound(SOUND_S2_ERROR); showOnMatrix("TIME UP"); fill_solid(leds, NUM_PIXELS, FAIL_COLOR); FastLED.show();
    currentState = GAME_OVER_TIMEOUT; gameTimerIsActive = false;
  }

  switch (currentState) {
    case WAITING_TO_START: if (digitalRead(BUTTON_1) == LOW) { currentState = STEP1_INIT; delay(200); } break;
    case STEP1_INIT:
      playSound(SOUND_S1_WELCOME); step1Level = 0; showOnMatrix("");
      gameTimerIsActive = true; gameStartTime = millis();
      resetStep1Round(); currentState = STEP1_PLAYING;
      break;
    case STEP1_PLAYING: handleStep1(); break;
    case STEP1_COMPLETE: playSound(SOUND_S1_END); showOnMatrix("DEAD"); stateTimer = millis(); currentState = WAITING_FOR_STEP2; break;
    case WAITING_FOR_STEP2: { static bool p=false; if (millis()-stateTimer > 4000 && !p) { playSound(SOUND_S2_READY); showOnMatrix("STEP 2?"); p=true; } if (p && digitalRead(BUTTON_1) == LOW) { step2Level=0; p=false; currentState=STEP2_INIT; delay(200); } } break;
    case STEP2_INIT: showOnMatrix(STEP2_LEVELS[step2Level]); stateTimer=millis(); for(int i=0; i<4; i++) button_s2_state[i]=false; currentState=STEP2_PLAYING; break;
    case STEP2_PLAYING: { static char lastD[5]=""; if(millis()-stateTimer>1000){ char d[5]="----"; if(digitalRead(BUTTON_1)==LOW)button_s2_state[0]=true;if(digitalRead(BUTTON_2)==LOW)button_s2_state[1]=true;if(digitalRead(BUTTON_3)==LOW)button_s2_state[2]=true;if(digitalRead(BUTTON_4)==LOW)button_s2_state[3]=true; for(int i=0;i<4;i++){if(button_s2_state[i])d[i]='X';} if(strcmp(d,lastD)!=0){showOnMatrix(d);strcpy(lastD,d);}} if (millis()-stateTimer > 6000){bool c=true; for(int i=0;i<4;i++){if((STEP2_LEVELS[step2Level][i]=='X'&&!button_s2_state[i])||(STEP2_LEVELS[step2Level][i]=='O'&&button_s2_state[i])){c=false;break;}} if(c){playSound(SOUND_S2_SUCCESS);showOnMatrix("OK!");step2Level++;if(step2Level>=4){currentState=GAME_WON;}else{delay(2000);currentState=STEP2_INIT;}}else{playSound(SOUND_S2_ERROR);showOnMatrix("FAIL");delay(2000);currentState=STEP2_INIT;} strcpy(lastD,"");}} break;
    case GAME_WON: gameTimerIsActive=false; { static bool d=false; if(!d){playSound(SOUND_FINISH);showOnMatrix("FINISH");d=true;} static unsigned long l=0; if(millis()-l>100){l=millis();static bool o=false;o=!o; if(o)fill_solid(leds,NUM_PIXELS,CRGB::Green);else fill_solid(leds,NUM_PIXELS,CRGB::Black);FastLED.show();}} break;
    case GAME_OVER_TIMEOUT: break;
  }
  
  static unsigned long lastUpdate = 0; if (millis() - lastUpdate > 500) { lastUpdate=millis(); notifyClients(); publishStatus(); }
}

//--- HELPER FUNCTIONS ---
void playSound(uint8_t track) { if (dfPlayerStatus) myDFPlayer.playFolder((soundLanguage == LANG_TR) ? 1 : 2, track); }
void showOnMatrix(const char* text) { strcpy(matrixBuffer, text); P.displayClear(); P.displayText(matrixBuffer, PA_CENTER, 0, 0, PA_NO_EFFECT, PA_NO_EFFECT); }

// ======================= GAME LOGIC FUNCTIONS =========================
void handleStep1() {
  if (digitalRead(BUTTON_1) == LOW) {
    if (alertIsActive && (millis() - alertTimer < currentReactionWindow)) {
      // --- BAŞARILI ---
      playSound(SOUND_S2_SUCCESS);
      step1Level++;
      
      // Matris ekranda D-E-A-D harflerini biriktirerek göster
      String pWord; 
      for(int i=0; i<step1Level; i++) {
        pWord += STEP1_TARGET_WORD[i];
      }
      showOnMatrix(pWord.c_str());

      if (step1Level >= 4) {
        currentState = STEP1_COMPLETE; 
      } else {
        resetStep1Round();
      }
    } else {
      // --- BAŞARISIZ (Erken basıldı) ---
      playSound(SOUND_S2_ERROR);
      showOnMatrix("START");
      fill_solid(leds, NUM_PIXELS, CRGB::Black);
      FastLED.show();
      gameTimerIsActive = false; 
      currentState = WAITING_TO_START;
    }
    delay(200); 
    return;
  }

  // Kırmızı ışık fırsatı kaçırıldı mı? (Zaman aşımı)
  if (alertIsActive && (millis() - alertTimer > currentReactionWindow)) {
    // --- FIRSAT KAÇTI ---
    // Hata verme, sadece animasyona devam etmesi için alert modunu kapat.
    alertIsActive = false;
  }

  // Animasyonu ilerlet
  if (millis() - stateTimer > currentAnimationInterval) {
    stateTimer = millis();
    ledProgress++;
    if (ledProgress >= NUM_PIXELS) {
      ledProgress = 0; 
    }
    
    fill_solid(leds, ledProgress, PROGRESS_COLOR);
    
    if (ledProgress == targetStep && !alertIsActive) {
      alertIsActive = true;
      alertTimer = millis();
    }
    
    if (alertIsActive) {
      leds[ledProgress] = ALERT_COLOR;
    }
    
    FastLED.show();
  }
}

void resetStep1Round() {
  currentAnimationInterval = BASE_ANIMATION_INTERVAL - (step1Level * 35);
  currentReactionWindow = BASE_REACTION_WINDOW - (step1Level * 60);
  if (currentAnimationInterval < 100) currentAnimationInterval = 100;
  if (currentReactionWindow < 250) currentReactionWindow = 250;
  
  ledProgress = 0;
  alertIsActive = false;
  targetStep = random(10, NUM_PIXELS - 2); 
  
  fill_solid(leds, NUM_PIXELS, CRGB::Black);
  FastLED.show();
  stateTimer = millis();
}

// ======================= COMMUNICATION FUNCTIONS =========================
String getStatusJSON() {
  static JSONVar gameStateJson; 
  gameStateJson["gameState"] = gameStateNames[currentState];
  char levelDetail[32];
  if(currentState == STEP1_PLAYING || currentState == STEP1_INIT){ sprintf(levelDetail,"Adim 1 - Seviye %d / 4", step1Level + 1); }
  else if(currentState == STEP2_PLAYING || currentState == STEP2_INIT){ sprintf(levelDetail,"Adim 2 - Seviye %d / 4", step2Level + 1); }
  else { strcpy(levelDetail, "--"); }
  gameStateJson["levelDetail"] = levelDetail;
  gameStateJson["matrix"] = String(matrixBuffer);
  gameStateJson["volume"] = gameVolume;
  gameStateJson["soundLanguage"] = (soundLanguage == LANG_TR) ? "TR" : "EN";
  gameStateJson["dfPlayerStatus"] = dfPlayerStatus;
  gameStateJson["wifiStatus"] = wifiStatus;
  if (gameTimerIsActive) {
    long remainingSeconds = (GAME_DURATION - (millis() - gameStartTime)) / 1000;
    gameStateJson["timer"] = remainingSeconds > 0 ? remainingSeconds : 0;
  } else {
    gameStateJson["timer"] = (currentState==WAITING_TO_START || currentState==GAME_WON) ? GAME_DURATION/1000 : 0;
  }
  return JSON.stringify(gameStateJson);
}
void processAction(JSONVar& jsonData) {
  if (jsonData.hasOwnProperty("action")) {
    String action = (const char*)jsonData["action"];
    if (action == "start_game" && currentState == WAITING_TO_START) { currentState = STEP1_INIT; }
    else if (action == "reset_game") { ESP.restart(); }
    else if (action == "set_volume") { gameVolume = (int)jsonData["value"]; myDFPlayer.volume(gameVolume); }
    else if (action == "set_sound_language") { soundLanguage = (String((const char*)jsonData["value"]) == "TR") ? LANG_TR : LANG_EN; myDFPlayer.stop(); }
  }
}
void notifyClients() { ws.textAll(getStatusJSON()); }
void publishStatus() { if (mqttClient.connected()) { mqttClient.publish(status_topic, getStatusJSON().c_str()); } }
void mqttCallback(char*t,byte*p,unsigned int l){String m;for(int i=0;i<l;i++)m+=(char)p[i];JSONVar j=JSON.parse(m);processAction(j);publishStatus();}
void reconnectMQTT() { while(!mqttClient.connected()){String c="ESP32_GameClient-"+String(random(0xffff),HEX);if(mqttClient.connect(c.c_str())){mqttClient.subscribe(command_topic);}else{delay(5000);}}}