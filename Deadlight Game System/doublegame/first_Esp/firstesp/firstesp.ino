/**
 * @file game_esp1_ultimate_final.ino
 * @brief The absolute final, definitive, and complete code for ESP1.
 * @version 15.0 - Final
 */


#define MQTT_MAX_PACKET_SIZE 2048 

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
const char* esp1_connection_topic = "esp/esp1/connection";

// Static IP Configuration
IPAddress local_IP(192, 168, 20, 52);
IPAddress gateway(192, 168, 20, 1);
IPAddress subnet(255, 255, 255, 0);

//--- HARDWARE & PINS ---
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

//--- GAME SETTINGS ---
#define PROGRESS_COLOR      CRGB::Green
#define ALERT_COLOR         CRGB::Red
#define FAIL_COLOR          CRGB::DarkOrange

int base_animation_interval = 250;
int base_reaction_window = 500;
int max_strikes = 3;
const unsigned long GAME_DURATION = 10 * 60 * 1000;

// Sound File Numbers
const uint8_t SOUND_S1_WELCOME = 1, SOUND_S1_END = 2, SOUND_S2_READY = 3, SOUND_S2_SUCCESS = 4, SOUND_S2_ERROR = 5;
const uint8_t SOUND_TRANSITION = 7;
const uint8_t SOUND_GRAND_FINALE = 8;

//--- GLOBAL OBJECTS & VARIABLES ---
const char STEP1_TARGET_WORD[] = "DEAD";
const char* STEP2_LEVELS[] = {"OXOX", "XOOX", "XXXO", "OOOX"};
enum GameLanguage { LANG_TR, LANG_EN };
enum GameState { WAITING_TO_START, TRANSITION_TO_GAME1, STEP1_INIT, STEP1_PLAYING, STEP1_COMPLETE, WAITING_FOR_STEP2, STEP2_INIT, STEP2_PLAYING, GAME_WON, GAME_OVER_TIMEOUT, GAME_OVER_STRIKES, BOTH_GAMES_WON };
const char* gameStateNames[] = {
  "Waiting for Start", "Transition...", "Step 1 Starting", "Step 1 Playing", "Step 1 Complete",
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
bool button_s2_state[4] = {false, false, false, false};
bool dfPlayerStatus = false, wifiStatus = false;
int gameVolume = 5;
uint8_t last_played_sound_track = 0;
char matrixBuffer[20] = "READY";
int ledProgress = 0;
int targetStep = 0;
bool alertIsActive = false;
unsigned long alertTimer = 0;
int currentAnimationInterval = 250;
int currentReactionWindow = 500;
unsigned long gameStartTime = 0;
bool gameTimerIsActive = false;
bool game1_is_complete = false;
bool game2_is_complete = false;

//--- FUNCTION PROTOTYPES ---
void playSound(uint8_t); void showOnMatrix(const char*); void handleStep1(); void resetStep1Round(); String getStatusJSON();
void notifyClients(); void publishStatus(String jsonStatus); void processAction(JSONVar&); void mqttCallback(char*, byte*, unsigned int); void reconnectMQTT();
void resetGame(bool fullSystemReset); void goToWaitingState(); void applySettings(JSONVar&);

// ======================= SETUP =========================
void setup() {
  Serial.begin(115200); randomSeed(analogRead(0));
  FastLED.addLeds<WS2811, LED_PIN, BRG>(leds, NUM_PIXELS); FastLED.setBrightness(BRIGHTNESS);
  fill_solid(leds, NUM_PIXELS, CRGB::Black); FastLED.show();
  P.begin(); P.setIntensity(4);
  mp3Serial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  if (myDFPlayer.begin(mp3Serial)) { myDFPlayer.volume(gameVolume); dfPlayerStatus = true; }
  pinMode(BUTTON_1, INPUT_PULLUP); pinMode(BUTTON_2, INPUT_PULLUP); pinMode(BUTTON_3, INPUT_PULLUP); pinMode(BUTTON_4, INPUT_PULLUP);
  if (!WiFi.config(local_IP, gateway, subnet)) { Serial.println("STA Failed to configure"); }
  WiFi.begin(wifi_ssid, wifi_password);
  Serial.print("Connecting to WiFi ");
  while (WiFi.status() != WL_CONNECTED && millis() < 10000) { delay(500); Serial.print("."); }
  Serial.println();
  wifiStatus = (WiFi.status() == WL_CONNECTED);
  if (wifiStatus) { Serial.print("WiFi Connected, IP: "); Serial.println(WiFi.localIP()); }
  else { Serial.println("WiFi connection failed."); }
  mqttClient.setBufferSize(2048);
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
    case WAITING_TO_START: break;
    case TRANSITION_TO_GAME1: {
        static unsigned long transitionStartTime = 0;
        if (transitionStartTime == 0) {
            transitionStartTime = millis();
            playSound(SOUND_TRANSITION);
            showOnMatrix("NEXT GAME");
        }
        if (millis() - transitionStartTime > 5000) {
            transitionStartTime = 0;
            currentState = STEP1_INIT;
        }
        break;
    }
    case STEP1_INIT:
      playerStrikes = 0;
      step1Level = 0;
      Serial.printf("[GAME] Step 1 starting. Strikes & Level reset.\n");
      if (!game1_is_complete && !game2_is_complete) { playSound(SOUND_S1_WELCOME); }
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
      static bool soundPlayed = false;
      if (winEntryTime == 0) {
        Serial.println("[GAME 1] WON! Checking status...");
        showOnMatrix("SUCCESS!");
        game1_is_complete = true;
        winEntryTime = millis();
        soundPlayed = false;
        if (game2_is_complete) {
            currentState = BOTH_GAMES_WON;
            winEntryTime = 0;
            return;
        }
      }
      if (millis() - winEntryTime > 2500 && !soundPlayed) {
          playSound(SOUND_TRANSITION);
          soundPlayed = true;
      }
      if (millis() - winEntryTime > 5000) {
        Serial.println("[TRANSITION] Sending command to start Game 2 on ESP2.");
        if (mqttClient.connected()) {
            mqttClient.publish("esp32-gamemaster/command", "{\"action\":\"start_mic_game\"}");
        }
        goToWaitingState();
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
        fill_solid(leds, NUM_PIXELS, FAIL_COLOR); FastLED.show();
        playSound(SOUND_S2_ERROR);
        gameTimerIsActive = false;
        gameOverMessageShown = true;
      }
      if (digitalRead(BUTTON_1) == LOW) {
        Serial.println("[SYSTEM] Full system restart triggered by button after Game Over.");
        delay(500);
        if(mqttClient.connected()) {
          mqttClient.publish("esp32-gamemaster/command", "{\"action\":\"reset_game\"}");
        }
        resetGame(true);
        gameOverMessageShown = false;
      }
      break;
    }
  }
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 500) {
    lastUpdate = millis();
    notifyClients();
  }
}

//--- HELPER FUNCTIONS ---
void playSound(uint8_t track) { if (dfPlayerStatus) { last_played_sound_track = track; myDFPlayer.playFolder((soundLanguage == LANG_TR) ? 1 : 2, track); } }
void showOnMatrix(const char* text) { strcpy(matrixBuffer, text); P.displayClear(); P.displayText(matrixBuffer, PA_CENTER, 0, 0, PA_NO_EFFECT, PA_NO_EFFECT); }
void goToWaitingState() {
    Serial.println("Transitioning to WAITING_TO_START state (memory kept).");
    fill_solid(leds, NUM_PIXELS, CRGB::Black); FastLED.show();
    P.displayClear(); showOnMatrix("READY"); myDFPlayer.stop();
    gameTimerIsActive = false;
    step1Level = 0; step2Level = 0; playerStrikes = 0;
    currentState = WAITING_TO_START;
}
void resetGame(bool fullSystemReset) {
    Serial.println("Resetting game...");
    if (fullSystemReset) {
      Serial.println("Full system reset: Clearing completion flags.");
      game1_is_complete = false;
      game2_is_complete = false;
    }
    goToWaitingState();
}

// ======================= GAME LOGIC FUNCTIONS =========================
void handleStep1() {
  if (digitalRead(BUTTON_1) == LOW) {
    delay(50); 
    if(digitalRead(BUTTON_1) == LOW){
      if (alertIsActive) {
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
        Serial.printf("[GAME] FAIL! Wrong time press. Strikes: %d / %d\n", playerStrikes, max_strikes);
        playSound(SOUND_S2_ERROR);
        if (playerStrikes >= max_strikes) { currentState = GAME_OVER_STRIKES; } 
        else { showOnMatrix("FAIL"); delay(1000); showOnMatrix(""); }
      }
      while(digitalRead(BUTTON_1) == LOW);
    }
    return;
  }
  if (millis() - stateTimer > currentAnimationInterval) {
    stateTimer = millis();
    ledProgress++;
    if (ledProgress >= NUM_PIXELS) {
      ledProgress = 0;
      if (alertIsActive) {
        alertIsActive = false;
        Serial.println("[GAME] Alert window missed, loop continues without penalty.");
      }
    }
    fill_solid(leds, NUM_PIXELS, CRGB::Black);
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
  currentAnimationInterval = base_animation_interval - (step1Level * 35);
  currentReactionWindow = base_reaction_window - (step1Level * 60);
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
  gameStateJson["soundLanguage"] = (soundLanguage == LANG_TR) ? "TR" : "EN";
  gameStateJson["wifiStatus"] = wifiStatus;
  gameStateJson["dfPlayerStatus"] = dfPlayerStatus;
  if (gameTimerIsActive) {
    long remainingSeconds = (GAME_DURATION - (millis() - gameStartTime)) / 1000;
    gameStateJson["timer"] = remainingSeconds > 0 ? remainingSeconds : 0;
  } else {
    gameStateJson["timer"] = (currentState == WAITING_TO_START) ? GAME_DURATION / 1000 : 0;
  }
  gameStateJson["game1_is_complete"] = game1_is_complete;
  gameStateJson["game2_is_complete"] = game2_is_complete;
  JSONVar settings;
  settings["volume"] = gameVolume;
  settings["base_animation_interval"] = base_animation_interval;
  settings["base_reaction_window"] = base_reaction_window;
  settings["max_strikes"] = max_strikes;
  gameStateJson["settings"] = settings;
  JSONVar hardwareStatus;
  hardwareStatus["button1_pressed"] = (digitalRead(BUTTON_1) == LOW);
  hardwareStatus["button2_pressed"] = (digitalRead(BUTTON_2) == LOW);
  hardwareStatus["button3_pressed"] = (digitalRead(BUTTON_3) == LOW);
  hardwareStatus["button4_pressed"] = (digitalRead(BUTTON_4) == LOW);
  hardwareStatus["lastSoundTrack"] = last_played_sound_track;
  gameStateJson["hardware"] = hardwareStatus;
  return JSON.stringify(gameStateJson);
}

void applySettings(JSONVar& newSettings) {
    if (newSettings.hasOwnProperty("volume")) { gameVolume = (int)newSettings["volume"]; myDFPlayer.volume(gameVolume); }
    if (newSettings.hasOwnProperty("base_animation_interval")) { base_animation_interval = (int)newSettings["base_animation_interval"]; }
    if (newSettings.hasOwnProperty("base_reaction_window")) { base_reaction_window = (int)newSettings["base_reaction_window"]; }
    if (newSettings.hasOwnProperty("max_strikes")) { max_strikes = (int)newSettings["max_strikes"]; }
    Serial.println("[SETTINGS] ESP1 settings updated.");
    notifyClients();
}

void processAction(JSONVar& jsonData) {
  if (jsonData.hasOwnProperty("action")) {
    String action = (const char*)jsonData["action"];
    if (action == "start_game") {
        if (currentState == WAITING_TO_START) {
            if (game2_is_complete) { currentState = TRANSITION_TO_GAME1; } 
            else { playSound(SOUND_S1_WELCOME); currentState = STEP1_INIT; }
        }
    } else if (action == "reset_game") {
        resetGame(true);
    } else if (action == "skip_led_game") {
        if (currentState >= STEP1_INIT && currentState < STEP1_COMPLETE) {
            Serial.println("[COMMAND] Skipping LED game (Step 1)...");
            step1Level = 4;
            currentState = STEP1_COMPLETE;
        }
    } else if (action == "skip_xoxo_game") {
        if (currentState >= WAITING_FOR_STEP2 && currentState < GAME_WON) {
            Serial.println("[COMMAND] Skipping XOXO game (Step 2)...");
            currentState = GAME_WON;
        }
    } else if (action == "replay_step1") {
        Serial.println("[COMMAND] Replaying Game 1...");
        resetGame(true);
        delay(100);
        JSONVar startCmd = JSON.parse("{\"action\":\"start_game\"}");
        processAction(startCmd);
    } else if (action == "force_win_game1") {
        Serial.println("[COMMAND] Forcing Game 1 win...");
        if(!game1_is_complete) {
            game1_is_complete = true;
            if(game2_is_complete) { currentState = BOTH_GAMES_WON; }
            else { currentState = GAME_WON; }
        }
    }
    else if (action == "set_volume") { gameVolume = (int)jsonData["value"]; myDFPlayer.volume(gameVolume); }
    else if (action == "set_sound_language") {
      String lang = (const char*)jsonData["value"];
      if (lang == "TR") { soundLanguage = LANG_TR; } else if (lang == "EN") { soundLanguage = LANG_EN; }
      myDFPlayer.stop();
    }
     else if (action == "apply_settings") {
        if(jsonData.hasOwnProperty("payload")) {
            JSONVar settingsPayload = jsonData["payload"];
            applySettings(settingsPayload);
        }
    }
  }
}
void notifyClients() {
    // Veriyi her seferinde yeniden oluşturmak yerine, tek seferde oluşturup
    // hem WebSocket'e hem de MQTT'ye gönderiyoruz.
    String statusJson = getStatusJSON();
    ws.textAll(statusJson); 
    publishStatus(statusJson);
}

void publishStatus(String jsonStatus) { 
    if (mqttClient.connected()) { 
        // String nesnesini char array'e kopyala
        // Bu, kütüphanenin daha stabil çalışmasını sağlar
        int len = jsonStatus.length() + 1;
        char message_buffer[len];
        jsonStatus.toCharArray(message_buffer, len);

        bool success = mqttClient.publish(game_status_topic, message_buffer, false); 
        
        if (!success) {
            Serial.println("[MQTT] !! ESP1 FAILED TO PUBLISH STATUS !!");
            Serial.printf("[MQTT] Client State: %d\n", mqttClient.state());
        }
    } else {
        Serial.println("[MQTT] Cannot publish, client not connected.");
    }
}

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
      if (masterMode == 2) { 
        if (currentState != WAITING_TO_START && currentState != BOTH_GAMES_WON && currentState != GAME_WON) {
          Serial.println("!!! Master ESP has re-homed. Resetting this ESP to idle. !!!");
          resetGame(true);
        }
      }
    }
    return;
  }
  if (strcmp(topic, game_control_topic) == 0) {
      JSONVar j = JSON.parse(message);
      processAction(j);
  }
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    String clientId = "ESP1_GameClient-" + String(random(0xffff), HEX);
    Serial.print("Attempting MQTT connection for ESP1...");
    if (mqttClient.connect(clientId.c_str(), nullptr, nullptr, esp1_connection_topic, 0, true, "offline")) {
      Serial.println("connected");
      mqttClient.publish(esp1_connection_topic, "online", true);
      mqttClient.subscribe(game_control_topic);
      mqttClient.subscribe(master_status_topic);
    } else {
      Serial.print("failed, rc="); Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}