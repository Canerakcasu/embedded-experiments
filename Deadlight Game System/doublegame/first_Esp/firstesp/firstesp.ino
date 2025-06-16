/**
 * @file game_esp1_final_complete.ino
 * @brief Deadlight Game System with all features and fixes.
 * @version 12.0 - Grand Finale
 */

//--- KÜTÜPHANELER ---
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
#include "index_h.h"

//--- NETWORK & MQTT CONFIGURATION ---
const char* wifi_ssid = "Ents_Test";
const char* wifi_password = "12345678";
const char* mqtt_server = "192.168.20.208";
const int   mqtt_port = 1883;
const char* game_control_topic = "game/control";
const char* game_status_topic = "game/status";
const char* master_status_topic = "esp32-gamemaster/status";

// Static IP Configuration
IPAddress local_IP(192, 168, 20, 52);
IPAddress gateway(192, 168, 20, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

//--- HARDWARE & GAME DEFINITIONS ---
#define LED_PIN             19
#define MATRIX_CLK_PIN      33
#define MATRIX_DATA_PIN     14
#define MATRIX_CS_PIN       27
#define BUTTON_1            32
#define BUTTON_2            4
#define BUTTON_3            12
#define BUTTON_4            13
#define DFPLAYER_RX_PIN     16
#define DFPLAYER_TX_PIN     17
#define NUM_PIXELS          23
#define BRIGHTNESS          80
#define MATRIX_HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MATRIX_MAX_DEVICES  4

// Step 1 Game Settings
#define PROGRESS_COLOR      CRGB::Green
#define ALERT_COLOR         CRGB::Red
#define FAIL_COLOR          CRGB::DarkOrange
#define BASE_ANIMATION_INTERVAL   250
#define BASE_REACTION_WINDOW      500
#define RESULT_SHOW_TIME    2000

// Game Timer
const unsigned long GAME_DURATION = 10 * 60 * 1000;

// Sound File Numbers
const uint8_t SOUND_S1_WELCOME = 1, SOUND_S1_END = 2, SOUND_S2_READY = 3, SOUND_S2_SUCCESS = 4, SOUND_S2_ERROR = 5;
const uint8_t SOUND_TRANSITION = 7;
const uint8_t SOUND_GRAND_FINALE = 8;

//--- GLOBAL OBJECTS & VARIABLES ---
const char STEP1_TARGET_WORD[] = "DEAD";
const char* STEP2_LEVELS[] = {"OXOX", "XOOX", "XXXO", "OOOX"};
enum GameLanguage { LANG_TR, LANG_EN };
enum GameState { WAITING_TO_START, STEP1_INIT, STEP1_PLAYING, STEP1_COMPLETE, WAITING_FOR_STEP2, STEP2_INIT, STEP2_PLAYING, GAME_WON, GAME_OVER_TIMEOUT, GAME_OVER_STRIKES, BOTH_GAMES_WON };
const char* gameStateNames[] = {
  "Waiting for Start", "Step 1 Starting", "Step 1 Playing", "Step 1 Complete",
  "Waiting for Step 2", "Step 2 Starting", "Step 2 Playing", "GAME 1 WON", "GAME OVER (TIME)", "GAME OVER (STRIKES)", "BOTH GAMES WON!"
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
int playerStrikes = 0;
const int MAX_STRIKES = 3;
bool button_s2_state[4] = {false, false, false, false};
bool dfPlayerStatus = false, wifiStatus = false;
int gameVolume = 5;
char matrixBuffer[20] = "READY";
int ledProgress = 0;
int targetStep = 0;
bool alertIsActive = false;
unsigned long alertTimer = 0;
int currentAnimationInterval = BASE_ANIMATION_INTERVAL;
int currentReactionWindow = BASE_REACTION_WINDOW;
unsigned long gameStartTime = 0;
bool gameTimerIsActive = false;
bool game1_is_complete = false;
bool game2_is_complete = false;

//--- FUNCTION PROTOTYPES ---
void playSound(uint8_t); void showOnMatrix(const char*); void handleStep1(); void resetStep1Round(); String getStatusJSON();
void notifyClients(); void publishStatus(); void processAction(JSONVar&); void mqttCallback(char*, byte*, unsigned int); void reconnectMQTT();

// ======================= SETUP =========================
void setup() {
  Serial.begin(115200); randomSeed(analogRead(0));
  FastLED.addLeds<WS2811, LED_PIN, BRG>(leds, NUM_PIXELS); FastLED.setBrightness(BRIGHTNESS);
  fill_solid(leds, NUM_PIXELS, CRGB::Black); FastLED.show();
  P.begin(); P.setIntensity(4);
  mp3Serial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  if (myDFPlayer.begin(mp3Serial)) { myDFPlayer.volume(gameVolume); dfPlayerStatus = true; }
  pinMode(BUTTON_1, INPUT_PULLUP); pinMode(BUTTON_2, INPUT_PULLUP); pinMode(BUTTON_3, INPUT_PULLUP); pinMode(BUTTON_4, INPUT_PULLUP);

  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) { Serial.println("STA Failed to configure"); }
  WiFi.begin(wifi_ssid, wifi_password);
  Serial.print("Connecting to WiFi ");
  while (WiFi.status() != WL_CONNECTED && millis() < 10000) { delay(500); Serial.print("."); }
  Serial.println();
  wifiStatus = (WiFi.status() == WL_CONNECTED);
  if (wifiStatus) { Serial.print("WiFi Connected, IP: "); Serial.println(WiFi.localIP()); }
  else { Serial.println("WiFi connection failed."); }

  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);

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
    playSound(SOUND_S2_ERROR);
    showOnMatrix("TIME UP");
    fill_solid(leds, NUM_PIXELS, FAIL_COLOR); FastLED.show();
    currentState = GAME_OVER_TIMEOUT;
    gameTimerIsActive = false;
    notifyClients();
  }

  switch (currentState) {
    case WAITING_TO_START:
      break;

    case STEP1_INIT:
      playerStrikes = 0;
      Serial.printf("[GAME] Step 1 starting. Strikes reset to %d\n", playerStrikes);
      playSound(SOUND_TRANSITION);
      delay(1000);
      showOnMatrix("");
      gameTimerIsActive = true;
      gameStartTime = millis();
      resetStep1Round();
      currentState = STEP1_PLAYING;
      break;

    case STEP1_PLAYING:
      handleStep1();
      break;

    case STEP1_COMPLETE:
      playSound(SOUND_S1_END);
      showOnMatrix("DEAD");
      stateTimer = millis();
      currentState = WAITING_FOR_STEP2;
      break;

    case WAITING_FOR_STEP2: {
      static bool prompt_shown = false;
      if (millis() - stateTimer > 4000 && !prompt_shown) {
        playSound(SOUND_S2_READY);
        showOnMatrix("STEP 2?");
        prompt_shown = true;
      }
      if (prompt_shown && digitalRead(BUTTON_1) == LOW) {
        Serial.println("Button 1 pressed, starting Step 2.");
        step2Level = 0;
        prompt_shown = false;
        currentState = STEP2_INIT;
        delay(200);
      }
      break;
    }

    case STEP2_INIT:
      showOnMatrix(STEP2_LEVELS[step2Level]);
      stateTimer = millis();
      for (int i = 0; i < 4; i++) button_s2_state[i] = false;
      currentState = STEP2_PLAYING;
      break;

    case STEP2_PLAYING: {
      static char lastD[5] = "";
      if (millis() - stateTimer > 1000) {
        char d[5] = "----";
        if (digitalRead(BUTTON_1) == LOW) button_s2_state[0] = true;
        if (digitalRead(BUTTON_2) == LOW) button_s2_state[1] = true;
        if (digitalRead(BUTTON_3) == LOW) button_s2_state[2] = true;
        if (digitalRead(BUTTON_4) == LOW) button_s2_state[3] = true;
        for (int i = 0; i < 4; i++) { if (button_s2_state[i]) d[i] = 'X'; }
        if (strcmp(d, lastD) != 0) { showOnMatrix(d); strcpy(lastD, d); }
      }
      if (millis() - stateTimer > 6000) {
        bool c = true;
        for (int i = 0; i < 4; i++) {
          if ((STEP2_LEVELS[step2Level][i] == 'X' && !button_s2_state[i]) || (STEP2_LEVELS[step2Level][i] == 'O' && button_s2_state[i])) {
            c = false; break;
          }
        }
        if (c) {
          playSound(SOUND_S2_SUCCESS); showOnMatrix("OK!");
          step2Level++;
          if (step2Level >= 4) { currentState = GAME_WON; }
          else { delay(2000); currentState = STEP2_INIT; }
        } else {
          playSound(SOUND_S2_ERROR); showOnMatrix("FAIL");
          delay(2000); currentState = STEP2_INIT;
        }
        strcpy(lastD, "");
      }
      break;
    }

    case GAME_WON: {
      static unsigned long winEntryTime = 0;
      if (winEntryTime == 0) {
        Serial.println("[GAME 1] WON! Waiting 5 seconds to start Game 2...");
        showOnMatrix("SUCCESS!");
        game1_is_complete = true;
        winEntryTime = millis();
        if (game2_is_complete) {
            currentState = BOTH_GAMES_WON;
            winEntryTime = 0;
            return;
        }
      }
      if (millis() - winEntryTime > 5000) {
        Serial.println("[TRANSITION] Sending command to start Game 2 on ESP2.");
        if (mqttClient.connected()) {
            mqttClient.publish("esp32-gamemaster/command", "{\"action\":\"start_mic_game\"}");
        }
        JSONVar resetCmd = JSON.parse("{\"action\":\"reset_game\"}");
        processAction(resetCmd);
        winEntryTime = 0;
      }
      break;
    }
    
    case BOTH_GAMES_WON: {
      static bool finaleMessageShown = false;
      if(!finaleMessageShown){
        Serial.println("!!! GRAND FINALE !!! Both games have been completed!");
        playSound(SOUND_GRAND_FINALE);
        showOnMatrix("YOU WIN");
        for(int i=0; i<3; i++) {
            fill_solid(leds, NUM_PIXELS, CRGB::Gold); FastLED.show(); delay(200);
            fill_solid(leds, NUM_PIXELS, CRGB::Black); FastLED.show(); delay(200);
        }
        fill_solid(leds, NUM_PIXELS, CRGB::Green); FastLED.show();
        finaleMessageShown = true;
      }
      break;
    }

    case GAME_OVER_TIMEOUT:
      break;
      
    case GAME_OVER_STRIKES: {
      static bool gameOverMessageShown = false;
      if (!gameOverMessageShown) {
        Serial.println("[GAME] Game Over due to 3 strikes.");
        showOnMatrix("GAME OVER");
        fill_solid(leds, NUM_PIXELS, FAIL_COLOR);
        FastLED.show();
        playSound(SOUND_S2_ERROR);
        gameTimerIsActive = false;
        gameOverMessageShown = true;
      }
      if (digitalRead(BUTTON_1) == LOW) {
        Serial.println("[SYSTEM] Restarting game from scratch after Game Over.");
        delay(500);
        JSONVar resetCmd = JSON.parse("{\"action\":\"reset_game\"}");
        processAction(resetCmd);
        gameOverMessageShown = false;
      }
      break;
    }
  }

  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 500) {
    lastUpdate = millis();
    notifyClients();
    publishStatus();
  }
}

//--- HELPER FUNCTIONS ---
void playSound(uint8_t track) { if (dfPlayerStatus) myDFPlayer.playFolder((soundLanguage == LANG_TR) ? 1 : 2, track); }
void showOnMatrix(const char* text) { strcpy(matrixBuffer, text); P.displayClear(); P.displayText(matrixBuffer, PA_CENTER, 0, 0, PA_NO_EFFECT, PA_NO_EFFECT); }

// ======================= GAME LOGIC FUNCTIONS =========================
void handleStep1() {
  if (digitalRead(BUTTON_1) == LOW) {
    if (alertIsActive && (millis() - alertTimer < currentReactionWindow)) {
      Serial.printf("[GAME] Level %d PASSED!\n", step1Level + 1);
      playSound(SOUND_S2_SUCCESS);
      step1Level++;
      String pWord;
      for (int i = 0; i < step1Level; i++) { pWord += STEP1_TARGET_WORD[i]; }
      showOnMatrix(pWord.c_str());
      if (step1Level >= 4) { currentState = STEP1_COMPLETE; }
      else { resetStep1Round(); }
    }
    else {
      playerStrikes++;
      Serial.printf("[GAME] FAIL! Early press. Strikes: %d / %d\n", playerStrikes, MAX_STRIKES);
      playSound(SOUND_S2_ERROR);
      if (playerStrikes >= MAX_STRIKES) { currentState = GAME_OVER_STRIKES; } 
      else { showOnMatrix("FAIL"); delay(1000); resetStep1Round(); }
    }
    delay(200); return;
  }
  if (alertIsActive && (millis() - alertTimer > currentReactionWindow)) {
    playerStrikes++;
    Serial.printf("[GAME] FAIL! Missed the alert. Strikes: %d / %d\n", playerStrikes, MAX_STRIKES);
    playSound(SOUND_S2_ERROR);
    if (playerStrikes >= MAX_STRIKES) { currentState = GAME_OVER_STRIKES; } 
    else { showOnMatrix("FAIL"); delay(1000); resetStep1Round(); }
    return;
  }
  if (millis() - stateTimer > currentAnimationInterval) {
    stateTimer = millis();
    ledProgress++;
    if (ledProgress >= NUM_PIXELS) { ledProgress = 0; }
    fill_solid(leds, NUM_PIXELS, CRGB::Black);
    fill_solid(leds, ledProgress, PROGRESS_COLOR);
    if (ledProgress == targetStep && !alertIsActive) { alertIsActive = true; alertTimer = millis(); }
    if (alertIsActive) { leds[ledProgress] = ALERT_COLOR; }
    FastLED.show();
  }
}
void resetStep1Round() {
  currentAnimationInterval = BASE_ANIMATION_INTERVAL - (step1Level * 35);
  currentReactionWindow = BASE_REACTION_WINDOW - (step1Level * 60);
  if (currentAnimationInterval < 100) currentAnimationInterval = 100;
  if (currentReactionWindow < 250) currentReactionWindow = 250;
  ledProgress = 0; alertIsActive = false;
  targetStep = random(10, NUM_PIXELS - 2);
  fill_solid(leds, NUM_PIXELS, CRGB::Black); FastLED.show();
  stateTimer = millis();
}

// ======================= COMMUNICATION FUNCTIONS =========================
String getStatusJSON() {
  JSONVar gameStateJson;
  gameStateJson["gameState"] = gameStateNames[currentState];
  char levelDetail[32];
  if (currentState >= STEP1_INIT && currentState < GAME_WON) {
      sprintf(levelDetail, "Step %d - Level %d / 4", (currentState < WAITING_FOR_STEP2 ? 1:2), (currentState < WAITING_FOR_STEP2 ? step1Level + 1 : step2Level + 1));
  } else { strcpy(levelDetail, "--"); }
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
    gameStateJson["timer"] = (currentState == WAITING_TO_START) ? GAME_DURATION / 1000 : 0;
  }
  return JSON.stringify(gameStateJson);
}

void processAction(JSONVar& jsonData) {
  if (jsonData.hasOwnProperty("action")) {
    String action = (const char*)jsonData["action"];
    if (action == "start_game") {
        if (currentState == WAITING_TO_START) {
            playSound(SOUND_TRANSITION);
            delay(1000);
            currentState = STEP1_INIT;
        } else if (currentState == WAITING_FOR_STEP2) {
            currentState = STEP2_INIT;
        }
    }
    else if (action == "reset_game") {
        Serial.println("Reset command received. Resetting game to idle.");
        fill_solid(leds, NUM_PIXELS, CRGB::Black);
        FastLED.show();
        P.displayClear();
        showOnMatrix("READY");
        myDFPlayer.stop();
        gameTimerIsActive = false;
        game1_is_complete = false;
        game2_is_complete = false;
        currentState = WAITING_TO_START;
    }
    else if (action == "set_volume") { gameVolume = (int)jsonData["value"]; myDFPlayer.volume(gameVolume); }
    else if (action == "set_sound_language") {
      String lang = (const char*)jsonData["value"];
      if (lang == "TR") { soundLanguage = LANG_TR; } else if (lang == "EN") { soundLanguage = LANG_EN; }
      myDFPlayer.stop();
    }
  }
}
void notifyClients() { ws.textAll(getStatusJSON()); }
void publishStatus() { if (mqttClient.connected()) { mqttClient.publish(game_status_topic, getStatusJSON().c_str(), true); } }

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) { message += (char)payload[i]; }
  Serial.print("MQTT Message Received ["); Serial.print(topic); Serial.print("]: "); Serial.println(message);

  if (strcmp(topic, master_status_topic) == 0) {
    JSONVar masterStatus = JSON.parse(message);
    if (masterStatus.hasOwnProperty("event") && String((const char*)masterStatus["event"]) == "game2_won") {
        Serial.println("[SYNC] Received Game 2 Won event from ESP2.");
        game2_is_complete = true;
        if(game1_is_complete) {
            currentState = BOTH_GAMES_WON;
        }
    }
    else if (masterStatus.hasOwnProperty("mode")) {
      int masterMode = (int)masterStatus["mode"];
      if (masterMode == 2) { // Corresponds to MODE_HOMING on ESP2
        if (currentState != WAITING_TO_START && currentState != GAME_WON && currentState != GAME_OVER_TIMEOUT && currentState != GAME_OVER_STRIKES && currentState != BOTH_GAMES_WON) {
          Serial.println("!!! Master ESP has re-homed. Resetting this ESP to idle. !!!");
          JSONVar resetCmd = JSON.parse("{\"action\":\"reset_game\"}");
          processAction(resetCmd);
        }
      }
    }
    return;
  }

  if (strcmp(topic, game_control_topic) == 0) {
      JSONVar j = JSON.parse(message);
      processAction(j);
      notifyClients();
      publishStatus();
  }
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    String c = "ESP32_GameClient-" + String(random(0xffff), HEX);
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect(c.c_str())) {
      Serial.println("connected");
      mqttClient.subscribe(game_control_topic);
      mqttClient.subscribe(master_status_topic);
    } else {
      Serial.print("failed, rc="); Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}