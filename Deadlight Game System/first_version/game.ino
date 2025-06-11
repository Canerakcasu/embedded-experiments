/**
 * @file game.ino
 * @brief Deadlight Game System - Interactive Escape Room Prop
 * @details This project creates a multi-stage game system using a WS2811 LED strip,
 * a dot matrix display, a DFPlayer Mini sound module, and buttons. The system
 * is also controllable via a web interface and a WebSocket server.
 * @version 2.5 (Final for this phase)
 * @date 11 June 2025
 */

// --- LIBRARIES ---
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

// --- HARDWARE & GAME DEFINITIONS ---
// Pin Definitions
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

// Hardware Settings
#define NUM_PIXELS           23   // Number of addressable pixels (69 LEDs / 3 LEDs per pixel)
#define BRIGHTNESS           80
#define MATRIX_HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MATRIX_MAX_DEVICES   4

// Step 1 Game Settings
#define PROGRESS_COLOR       CRGB::Blue
#define ALERT_COLOR          CRGB::Red
#define SUCCESS_COLOR        CRGB::LawnGreen
#define FAIL_COLOR           CRGB::DarkOrange
#define ANIMATION_INTERVAL   250  // ms
#define REACTION_WINDOW      500  // ms
#define RESULT_SHOW_TIME     2000 // ms
#define CHUNK_SIZE           1    // 1 pixel = 1 group of 3 LEDs

// Sound File Numbers
const uint8_t SOUND_S1_WELCOME   = 1;
const uint8_t SOUND_S1_END       = 2;
const uint8_t SOUND_S2_READY     = 3;
const uint8_t SOUND_S2_SUCCESS   = 4;
const uint8_t SOUND_S2_ERROR     = 5;

//--- GLOBAL OBJECTS & VARIABLES ---

// Game Content
const char STEP1_TARGET_WORD[] = "DEAD";
const char* STEP2_LEVELS[] = {"OXOX", "XOOX", "XXXO", "OOOX"};

// Game State Enums
enum GameLanguage { LANG_TR, LANG_EN };
enum GameState {
  WAITING_TO_START, STEP1_INIT, STEP1_PLAYING, STEP1_COMPLETE,
  WAITING_FOR_STEP2, STEP2_INIT, STEP2_PLAYING, GAME_WON
};
enum Step1Phase { S1_PLAYING, S1_SHOWING_SUCCESS, S1_SHOWING_FAILURE };

// Hardware Objects
CRGB leds[NUM_PIXELS];
MD_Parola P = MD_Parola(MATRIX_HARDWARE_TYPE, MATRIX_DATA_PIN, MATRIX_CLK_PIN, MATRIX_CS_PIN, MATRIX_MAX_DEVICES);
HardwareSerial mp3Serial(2);
DFRobotDFPlayerMini myDFPlayer;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// State Variables
GameState currentState = WAITING_TO_START;
Step1Phase currentStep1Phase = S1_PLAYING;
GameLanguage currentGameLanguage = LANG_TR;
unsigned long stateTimer = 0;
int step1Level = 0;
int step2Level = 0;
bool dfPlayerStatus = false;
bool wifiStatus = false;
int gameVolume = 5;
char matrixBuffer[20] = "START";

// Step 1 Specific Variables
int ledProgress = 0;
int currentStep = 0;
int targetStep = 0;
bool alertIsActive = false;
unsigned long alertTimer = 0;

//--- HELPER FUNCTION PROTOTYPES ---
void playSound(uint8_t track);
void showOnMatrix(const char* text);
void handleStep1();
void resetStep1Round();
void notifyClients();
void onEvent(AsyncWebSocket*, AsyncWebSocketClient*, AwsEventType, void*, uint8_t*, size_t);

// ======================= SETUP =========================
void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(0));

  // Init LED Strip
  FastLED.addLeds<WS2811, LED_PIN, GRB>(leds, NUM_PIXELS);
  FastLED.setBrightness(BRIGHTNESS);
  fill_solid(leds, NUM_PIXELS, CRGB::Black);
  FastLED.show();

  // Init Matrix Display
  P.begin();
  P.setIntensity(4);

  // Init Sound Module
  mp3Serial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  if (myDFPlayer.begin(mp3Serial)) {
    myDFPlayer.volume(gameVolume);
    dfPlayerStatus = true;
  } else {
    dfPlayerStatus = false;
  }

  // Init Buttons
  pinMode(BUTTON_1, INPUT_PULLUP);
  pinMode(BUTTON_2, INPUT_PULLUP);
  pinMode(BUTTON_3, INPUT_PULLUP);
  pinMode(BUTTON_4, INPUT_PULLUP);

  // Init WiFi (Non-blocking)
  WiFi.begin("Ents_Test", "12345678");
  int wifi_retries = 0;
  while (WiFi.status() != WL_CONNECTED && wifi_retries < 20) {
    delay(500);
    wifi_retries++;
  }
  wifiStatus = (WiFi.status() == WL_CONNECTED);
  
  // Init Web Server & WebSocket
  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ request->send_P(200, "text/html", index_html); });
  server.begin();
  
  showOnMatrix(matrixBuffer);
}

// ======================= MAIN LOOP =========================
void loop() {
  if (P.displayAnimate()) P.displayReset();
  ws.cleanupClients();

  switch (currentState) {
    case WAITING_TO_START:
      if (digitalRead(BUTTON_1) == LOW) { playSound(SOUND_S1_WELCOME); currentState = STEP1_INIT; delay(200); }
      break;
    case STEP1_INIT:
      step1Level = 0;
      showOnMatrix("");
      resetStep1Round();
      currentState = STEP1_PLAYING;
      break;
    case STEP1_PLAYING:
      handleStep1();
      break;
    case STEP1_COMPLETE:
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
      if (millis() - stateTimer > 6000) { // 1s show + 5s input
        bool correct = true;
        for (int i = 0; i < 4; i++) {
          if ((STEP2_LEVELS[step2Level][i] == 'X' && !button_s2_state[i]) ||
              (STEP2_LEVELS[step2Level][i] == 'O' && button_s2_state[i])) {
            correct = false; break;
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
  // Update web clients periodically
  static unsigned long lastNotify = 0;
  if (millis() - lastNotify > 500) {
   lastNotify = millis();
   notifyClients();
  }
}

// ======================= GAME LOGIC FUNCTIONS =========================

/**
 * @brief Handles the logic for Step 1 (Reaction Game).
 * The animation runs continuously until a correct or incorrect button press.
 */
void handleStep1() {
  switch (currentStep1Phase) {
    case S1_PLAYING:
      // Check for button press at any time
      if (digitalRead(BUTTON_1) == LOW) {
        // Was it a valid press (during the red light alert)?
        if (alertIsActive && (millis() - alertTimer < REACTION_WINDOW)) {
          // --- SUCCESS ---
          step1Level++;
          playSound(SOUND_S2_SUCCESS);
          fill_solid(leds, NUM_PIXELS, SUCCESS_COLOR);
          FastLED.show();
          String pWord; 
          for(int i=0; i<step1Level; i++) pWord += STEP1_TARGET_WORD[i];
          showOnMatrix(pWord.c_str());
          stateTimer = millis();
          currentStep1Phase = S1_SHOWING_SUCCESS;
        } else {
          // --- FAILURE (Pressed too early or too late) ---
          playSound(SOUND_S2_ERROR);
          fill_solid(leds, NUM_PIXELS, FAIL_COLOR);
          FastLED.show();
          showOnMatrix("FAIL!");
          stateTimer = millis();
          currentStep1Phase = S1_SHOWING_FAILURE;
        }
        delay(200); // Debounce
        return;
      }

      // Animate the LED strip
      if (millis() - stateTimer > ANIMATION_INTERVAL) {
        stateTimer = millis();
        ledProgress += CHUNK_SIZE;
        currentStep++;

        // Wrap around animation if it reaches the end
        if (ledProgress >= NUM_PIXELS) {
          ledProgress = 0;
          currentStep = 0; 
        }
        
        // Expire the alert window if time is up
        if (alertIsActive && (millis() - alertTimer >= REACTION_WINDOW)) {
          alertIsActive = false;
        }

        fill_solid(leds, NUM_PIXELS, CRGB::Black);
        
        CRGB blockColor = PROGRESS_COLOR;
        
        // Start the alert period if the target step is reached
        if (currentStep == targetStep && !alertIsActive) {
          alertIsActive = true;
          alertTimer = millis();
        }

        // If in alert period, draw with the alert color
        if (alertIsActive) {
          blockColor = ALERT_COLOR;
        }

        // Draw the moving block
        for (int i = 0; i < CHUNK_SIZE; i++) {
          if (ledProgress + i < NUM_PIXELS) leds[ledProgress + i] = blockColor;
        }
        FastLED.show();
      }
      break;

    case S1_SHOWING_SUCCESS:
      // Show success message/color for a duration, then start next round or finish
      if (millis() - stateTimer > RESULT_SHOW_TIME) {
        alertIsActive = false;
        if (step1Level >= 4) {
          currentState = STEP1_COMPLETE;
        } else {
          resetStep1Round();
        }
      }
      break;
      
    case S1_SHOWING_FAILURE:
      // Show failure message/color for a duration, then reset game
      if (millis() - stateTimer > RESULT_SHOW_TIME) {
        alertIsActive = false;
        showOnMatrix("START");
        fill_solid(leds, NUM_PIXELS, CRGB::Black);
        FastLED.show();
        currentState = WAITING_TO_START;
      }
      break;
  }
}

/**
 * @brief Resets the state for a new round of the Step 1 game.
 */
void resetStep1Round() {
  ledProgress = -CHUNK_SIZE;
  currentStep = 0;
  alertIsActive = false;

  int totalStepsInLoop = NUM_PIXELS / CHUNK_SIZE;
  targetStep = random(4, totalStepsInLoop - 2); 
  
  fill_solid(leds, NUM_PIXELS, CRGB::Black);
  FastLED.show();
  currentStep1Phase = S1_PLAYING;
  stateTimer = millis();
}

// ======================= WEB SERVER & WEBSOCKET FUNCTIONS =========================

/**
 * @brief Sends the current game state to all connected web clients via WebSocket.
 */
void notifyClients() {
  static JSONVar gameStateJson; 
  
  gameStateJson["gameState"] = gameStateNames[currentState];
  
  // Create a detailed level string
  char levelDetail[32];
  if (currentState == STEP1_PLAYING || currentState == STEP1_INIT) {
    sprintf(levelDetail, "Step 1 - Letter %d / 4", step1Level + 1);
  } else if (currentState == STEP2_PLAYING || currentState == STEP2_INIT) {
    sprintf(levelDetail, "Step 2 - Level %d / 4", step2Level + 1);
  } else {
    strcpy(levelDetail, "--");
  }
  gameStateJson["levelDetail"] = levelDetail;

  // Add other state data
  gameStateJson["matrix"] = String(matrixBuffer);
  gameStateJson["volume"] = gameVolume;
  gameStateJson["language"] = (currentGameLanguage == LANG_TR) ? "TR" : "EN";
  gameStateJson["dfPlayerStatus"] = dfPlayerStatus;
  gameStateJson["wifiStatus"] = wifiStatus;

  String jsonString = JSON.stringify(gameStateJson);
  ws.textAll(jsonString);
}

/**
 * @brief Handles incoming WebSocket events (connect, disconnect, data).
 */
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
          return; // Ignore invalid JSON
        }
        if (jsonData.hasOwnProperty("action")) {
          String action = (const char*)jsonData["action"];
          
          if (action == "start_game" && currentState == WAITING_TO_START) {
            playSound(SOUND_S1_WELCOME);
            currentState = STEP1_INIT;
          }
          else if (action == "reset_game") {
            showOnMatrix("RESET");
            delay(1000);
            ESP.restart();
          }
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
          
          notifyClients(); // Send updated state back to clients
        }
      }
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}