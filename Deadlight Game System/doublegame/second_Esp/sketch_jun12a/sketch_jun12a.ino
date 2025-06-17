/**
 * @file esp32_master_controller_ultimate_final.ino
 * @brief The absolute final, definitive, and complete code for ESP2.
 * @version 16.3 - Final
 */

#define MQTT_MAX_PACKET_SIZE 2048

#include <WiFi.h>
#include <PubSubClient.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Arduino_JSON.h>
#include <cmath>
#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_NeoPixel.h>
#include <AccelStepper.h>
#include "index_h.h"

// ====================================================================================================
//                                      TANIMLAMALAR VE AYARLAR
// ====================================================================================================

// --- Donanım Pinleri ---
#define RFID_SS_PIN           15
#define RFID_RST_PIN          3
#define STEPPER_DIR_PIN       22
#define STEPPER_STEP_PIN      12
#define STEPPER_ENABLE_PIN    21
#define POWER_ENABLE_PIN      25
#define VOLTMETER_CONTROL_PIN 13
#define GAME1_BUTTON_PIN      27
#define LED_PIN               4
#define DIGITAL_MIC_PIN       32

// --- WiFi & Ağ Ayarları ---
const char* ssid = "Ents_Test";
const char* password = "12345678";
IPAddress staticIP(192, 168, 20, 53);
IPAddress gateway(192, 168, 20, 1);
IPAddress subnet(255, 255, 255, 0);

// --- MQTT Ayarları ---
const char* mqtt_server = "192.168.20.208";
const char* mqtt_client_id = "esp32_game_master";
const char* topic_game_control = "game/control";
const char* mqtt_command_topic = "esp32-gamemaster/command";
const char* mqtt_status_topic = "esp32-gamemaster/status";
const char* mqtt_settings_topic = "esp32-gamemaster/settings/set";
const char* esp2_connection_topic = "esp/esp2/connection";

// --- Web Sunucusu ve WebSocket Nesneleri ---
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
WiFiClient espClient;
PubSubClient client(espClient);

// --- Kontrol Edilebilir Ayarlar ---
long game1_selectionInterval = 50;
int STEPS_PER_360_DEG = 200;
int STEPS_PER_90_DEG = STEPS_PER_360_DEG / 4;
int stepper_max_speed = 400;
int stepper_acceleration = 200;
int stepper_search_speed = 25;
int brightness_decrease_step = 20;
int brightness_increase_step = 10;
const int MIC_LOOP_DELAY_MS = 20;
int game1_debounceDelay = 500;

// --- Oyun Durumları ---
enum GameMode { MODE_IDLE = 0, MODE_MANUAL_CONTROL, MODE_HOMING, MODE_GAME1_VOLTMETER_SELECT, MODE_STEPPER_MOVING, MODE_AWAITING_AUTO_RFID, MODE_GAME2_ARGB_MICROPHONE, MODE_GAME2_WON };
GameMode currentGameMode = MODE_HOMING;
String last_rfid_uid = "None";
enum HomingState { SEARCHING_FOR_CARD, SEARCHING_FOR_EDGE, MOVING_TO_CENTER, HOMING_COMPLETE };
HomingState currentHomingState = SEARCHING_FOR_CARD;
long card_detect_start_pos = 0;
long card_detect_end_pos = 0;

// --- Nesneler ve Diğer Değişkenler ---
AccelStepper stepper(AccelStepper::DRIVER, STEPPER_STEP_PIN, STEPPER_DIR_PIN);
MFRC522 mfrc522(RFID_SS_PIN, RFID_RST_PIN);
Adafruit_NeoPixel strip(1, LED_PIN, NEO_GRB + NEO_KHZ800);
const int pwmFrequency = 5000;
const int pwmChannel = 0;
const int pwmResolution = 12;
float game1_targetVoltage = 0.0;
float game1_voltageDirection = 0.1;
int game1_selectionDutyCycle = 0;
unsigned long game1_lastSelectionUpdateTime = 0;
unsigned long game1_lastButtonPressTime = 0;
int game1_selectedLevel = 0;
bool wifiStatus = false;

// Kalibrasyon Verileri
int calibrated_voltage_points[] = {  0, 200, 400, 600, 700, 800, 1000 };
int calibrated_pwm_points[]     = {  0,  27,  31,  37,  65, 117,  220 };
int calibration_points_count = sizeof(calibrated_voltage_points) / sizeof(int);

//--- FONKSİYON PROTOTİPLERİ ---
void notifyClients(); String getStatusJSON(); void handleWebSocketMessage(void *arg, uint8_t *data, size_t len); void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void runPreciseHoming(); void runGame1_VoltmeterSelect(); void runStepperMotor(); void runStepperSearchAndRFID(); void runGame2_ARGB_Microphone(); void runIdleMode();
void resetAndStartVoltmeterGame(); void handleCardAndStartGame(); bool compareUIDs(byte* uid1, const byte* uid2, int size); void runGame2WinSequence(); int multiMap(int val, int* in, int* out, int size);
void reconnect(); void mqttCallback(char* topic, byte* payload, unsigned int length); void startMicrophoneGame(); void setIdleMode();
void applySettings(JSONVar& newSettings);


// ======================= SETUP =========================
void setup() {
    Serial.begin(115200);
    Serial.println("\n\n[SETUP] ESP2 Master Controller Starting...");
    pinMode(POWER_ENABLE_PIN, OUTPUT);
    pinMode(GAME1_BUTTON_PIN, INPUT_PULLUP);
    pinMode(STEPPER_ENABLE_PIN, OUTPUT);
    pinMode(DIGITAL_MIC_PIN, INPUT);
    digitalWrite(POWER_ENABLE_PIN, LOW);
    digitalWrite(STEPPER_ENABLE_PIN, LOW);

    SPI.begin();
    mfrc522.PCD_Init();
    strip.begin();
    strip.show();
    
    ledcSetup(pwmChannel, pwmFrequency, pwmResolution);
    ledcAttachPin(VOLTMETER_CONTROL_PIN, pwmChannel);

    WiFi.config(staticIP, gateway, subnet);
    WiFi.begin(ssid, password);
    Serial.print("[SETUP] Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED && millis() < 10000) { delay(500); Serial.print("."); }
    Serial.println();
    wifiStatus = (WiFi.status() == WL_CONNECTED);
    if(wifiStatus) { Serial.print("[SETUP] WiFi Connected. IP Address: "); Serial.println(WiFi.localIP()); }
    else { Serial.println("[SETUP] WiFi connection failed."); }

    client.setBufferSize(2048);
    client.setServer(mqtt_server, 1883);
    client.setCallback(mqttCallback);

    ws.onEvent(onEvent);
    server.addHandler(&ws);
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html, nullptr);
    });
    server.begin();
    Serial.println("[SETUP] Web server started.");
    Serial.println("[SETUP] Setup complete. Entering Homing mode...");
}

// ======================= MAIN LOOP =========================
void loop() {
    ws.cleanupClients();
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    switch (currentGameMode) {
        case MODE_IDLE:                   runIdleMode(); break;
        case MODE_HOMING:                 runPreciseHoming(); break;
        case MODE_GAME1_VOLTMETER_SELECT: runGame1_VoltmeterSelect(); break;
        case MODE_STEPPER_MOVING:         runStepperMotor(); break;
        case MODE_AWAITING_AUTO_RFID:     runStepperSearchAndRFID(); break;
        case MODE_GAME2_ARGB_MICROPHONE:  runGame2_ARGB_Microphone(); break;
        case MODE_GAME2_WON:              runGame2WinSequence(); break;
        case MODE_MANUAL_CONTROL:
            if (stepper.distanceToGo() == 0) {
                digitalWrite(STEPPER_ENABLE_PIN, HIGH);
                setIdleMode();
            } else {
                stepper.run();
            }
            break;
    }
    
    static unsigned long lastNotifyTime = 0;
    if (millis() - lastNotifyTime > 500) {
        lastNotifyTime = millis();
        notifyClients();
    }
}

// ======================= İLETİŞİM FONKSİYONLARI =========================
void notifyClients() { 
    String statusJson = getStatusJSON();
    if(ws.count() > 0){ 
        ws.textAll(statusJson); 
    }
    if(client.connected()) {
        bool success = client.publish(mqtt_status_topic, statusJson.c_str(), false);
        if (!success) {
            Serial.println("[MQTT] !! ESP2 FAILED TO PUBLISH STATUS !!");
        }
    }
}

String getStatusJSON() {
    JSONVar status;
    const char* mode_text;
    switch(currentGameMode){
        case MODE_IDLE:                   mode_text = "Idle"; break;
        case MODE_MANUAL_CONTROL:         mode_text = "Manual Control"; break;
        case MODE_HOMING:                 mode_text = "Homing..."; break;
        case MODE_GAME1_VOLTMETER_SELECT: mode_text = "Voltmeter Selection"; break;
        case MODE_STEPPER_MOVING:         mode_text = "Motor Moving to Target"; break;
        case MODE_AWAITING_AUTO_RFID:     mode_text = "Searching for RFID..."; break;
        case MODE_GAME2_ARGB_MICROPHONE:  mode_text = "Microphone Game Active"; break;
        case MODE_GAME2_WON:              mode_text = "Game 2 Won!"; break;
        default:                          mode_text = "Unknown State";
    }
    status["gameMode"] = mode_text;

    JSONVar hardware;
    hardware["buttonState"] = (digitalRead(GAME1_BUTTON_PIN) == LOW) ? "Pressed" : "Released";
    hardware["micStatus"] = (digitalRead(DIGITAL_MIC_PIN) == HIGH) ? "ACTIVE (Sound)" : "Passive";
    hardware["rfidStatus"] = last_rfid_uid;
    hardware["ledBrightness"] = strip.getBrightness();
    hardware["voltmeterPower"] = (digitalRead(POWER_ENABLE_PIN) == HIGH) ? "ON" : "OFF";
    hardware["liveVoltage"] = game1_targetVoltage;
    hardware["selectedLevel"] = game1_selectedLevel;
    hardware["motorPosition"] = stepper.currentPosition();
    hardware["motorSpeed"] = stepper.speed();
    hardware["motorRunning"] = (stepper.distanceToGo() != 0);
    hardware["wifiStatus"] = wifiStatus;
    status["hardware"] = hardware;
    
    JSONVar settings;
    settings["voltmeterSpeed"] = game1_selectionInterval;
    settings["stepsPer360"] = STEPS_PER_360_DEG;
    settings["stepperSpeed"] = stepper_max_speed;
    settings["stepperAccel"] = stepper_acceleration;
    settings["stepperSearchSpeed"] = stepper_search_speed;
    settings["micDecreaseStep"] = brightness_decrease_step;
    settings["micIncreaseStep"] = brightness_increase_step;
    status["settings"] = settings;
    
    return JSON.stringify(status);
}

void applySettings(JSONVar& newSettings) {
    if (newSettings.hasOwnProperty("voltmeterSpeed")) game1_selectionInterval = (long)newSettings["voltmeterSpeed"];
    if (newSettings.hasOwnProperty("stepsPer360")) { STEPS_PER_360_DEG = (int)newSettings["stepsPer360"]; STEPS_PER_90_DEG = STEPS_PER_360_DEG / 4; }
    if (newSettings.hasOwnProperty("stepperSpeed")) { stepper_max_speed = (int)newSettings["stepperSpeed"]; stepper.setMaxSpeed(stepper_max_speed); }
    if (newSettings.hasOwnProperty("stepperAccel")) { stepper_acceleration = (int)newSettings["stepperAccel"]; stepper.setAcceleration(stepper_acceleration); }
    if (newSettings.hasOwnProperty("stepperSearchSpeed")) stepper_search_speed = (int)newSettings["stepperSearchSpeed"];
    if (newSettings.hasOwnProperty("micDecreaseStep")) brightness_decrease_step = (int)newSettings["micDecreaseStep"];
    if (newSettings.hasOwnProperty("micIncreaseStep")) brightness_increase_step = (int)newSettings["micIncreaseStep"];
    Serial.println("[SETTINGS] ESP2 settings updated.");
    notifyClients();
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        JSONVar cmd = JSON.parse((char*)data);
        if (JSON.typeof(cmd) == "undefined") { return; }
        String action = (const char*)cmd["action"];
        if (action == "get_status") {
            notifyClients();
        }
        else if (action == "startGame") { 
            setIdleMode(); 
            delay(100); 
            currentGameMode = MODE_HOMING; 
            currentHomingState = SEARCHING_FOR_CARD;
        }
        else if (action == "resetSystem") { ESP.restart(); }
        else if (action == "moveMotor") {
            currentGameMode = MODE_MANUAL_CONTROL;
            digitalWrite(STEPPER_ENABLE_PIN, LOW);
            stepper.move((int)cmd["steps"]);
        }
        else if (action == "apply_settings") {
            if(cmd.hasOwnProperty("payload")) {
                JSONVar settingsPayload = cmd["payload"];
                applySettings(settingsPayload);
            }
        }
    }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if(type == WS_EVT_CONNECT){ Serial.printf("WS Client #%u connected\n", client->id()); notifyClients(); }
    else if(type == WS_EVT_DISCONNECT){ Serial.printf("WS Client #%u disconnected\n", client->id()); }
    else if(type == WS_EVT_DATA){ handleWebSocketMessage(arg, data, len); }
}

void reconnect() {
    while (!client.connected()) {
        String clientId = "ESP2_MasterClient-" + String(random(0xffff), HEX);
        Serial.print("Attempting MQTT connection for ESP2...");
        if (client.connect(clientId.c_str(), nullptr, nullptr, esp2_connection_topic, 0, true, "offline")) {
            Serial.println("connected");
            client.publish(esp2_connection_topic, "online", true);
            client.subscribe(mqtt_command_topic);
            client.subscribe(mqtt_settings_topic);
        } else {
            Serial.print("failed, rc="); Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String message;
    for (int i = 0; i < length; i++) { message += (char)payload[i]; }
    Serial.printf("MQTT Message arrived on topic: %s. Payload: %s\n", topic, message.c_str());
    if (strcmp(topic, mqtt_command_topic) == 0) {
        JSONVar cmd = JSON.parse(message);
        if (JSON.typeof(cmd) == "undefined") { return; }
        String action = (const char*)cmd["action"];
        if (action == "start_mic_game" || action == "replay_mic_game") { startMicrophoneGame(); } 
        else if (action == "reset_game") { setIdleMode(); delay(100); currentGameMode = MODE_HOMING; currentHomingState = SEARCHING_FOR_CARD; } 
        else if (action == "skip_mic_game") { Serial.println("[COMMAND] Skipping mic game..."); currentGameMode = MODE_GAME2_WON; }
        else if (action == "force_win_game2") {
            Serial.println("[COMMAND] Forcing Game 2 win...");
            if (client.connected()) { client.publish(mqtt_status_topic, "{\"event\":\"game2_won\"}"); }
            setIdleMode();
        }
    } else if (strcmp(topic, mqtt_settings_topic) == 0) {
        JSONVar settings = JSON.parse(message);
        applySettings(settings);
    }
}


// ======================= OYUN VE YARDIMCI FONKSİYONLAR =========================
int multiMap(int val, int* in, int* out, int size) {
    if (val <= in[0]) return out[0];
    if (val >= in[size - 1]) return out[size - 1];
    int pos = 1;
    while (val > in[pos]) pos++;
    if (val == in[pos]) return out[pos];
    return map(val, in[pos - 1], in[pos], out[pos - 1], out[pos]);
}

bool compareUIDs(byte* uid1, const byte* uid2, int size) { for (int i = 0; i < size; i++) { if (uid1[i] != uid2[i]) { return false; } } return true; }

void resetAndStartVoltmeterGame() {
    Serial.println("[ACTION] Starting Voltmeter game...");
    if (client.connected()) { client.publish(topic_game_control, "{\"action\":\"reset_game\"}"); }
    digitalWrite(POWER_ENABLE_PIN, HIGH);
    stepper.setMaxSpeed(stepper_max_speed);
    stepper.setAcceleration(stepper_acceleration);
    game1_targetVoltage = 0.0;
    game1_voltageDirection = 0.1;
    game1_lastSelectionUpdateTime = millis();
    strip.setBrightness(0);
    strip.show();
    currentGameMode = MODE_GAME1_VOLTMETER_SELECT;
}

void handleCardAndStartGame() {
    if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) { return; }
    stepper.stop();
    digitalWrite(STEPPER_ENABLE_PIN, HIGH);
    String uid_str = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) { if (mfrc522.uid.uidByte[i] < 0x10) uid_str += "0"; uid_str += String(mfrc522.uid.uidByte[i], HEX); }
    uid_str.toUpperCase();
    last_rfid_uid = uid_str;
    const byte CARD_UID_GAME1[] = {0x75, 0x41, 0x61, 0x9A};
    const byte CARD_UID_GAME2[] = {0xFA, 0x44, 0xA9, 0x00};
    if (compareUIDs(mfrc522.uid.uidByte, CARD_UID_GAME2, 4)) {
        startMicrophoneGame();
    } else if (compareUIDs(mfrc522.uid.uidByte, CARD_UID_GAME1, 4)) {
        if (!client.connected()) reconnect();
        char jsonPayload[50];
        sprintf(jsonPayload, "{\"action\":\"start_game\", \"level\":%d}", game1_selectedLevel);
        client.publish(topic_game_control, jsonPayload);
        Serial.printf("[MQTT] Command sent: %s\n", jsonPayload);
        currentGameMode = MODE_HOMING;
        currentHomingState = SEARCHING_FOR_CARD;
    } else {
        currentGameMode = MODE_HOMING;
        currentHomingState = SEARCHING_FOR_CARD;
    }
    mfrc522.PICC_HaltA();
    notifyClients();
}

void runPreciseHoming() {
    switch (currentHomingState) {
        case SEARCHING_FOR_CARD: {
            static bool entry_log = true;
            if(entry_log){
                Serial.println("[STATE] Homing: Searching for card...");
                stepper.setAcceleration(50);
                stepper.setMaxSpeed(100);
                stepper.setSpeed(50);
                digitalWrite(STEPPER_ENABLE_PIN, LOW);
                entry_log = false;
            }
            stepper.runSpeed();
            if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
                card_detect_start_pos = stepper.currentPosition();
                Serial.printf("[HOMING] Card leading edge found at: %ld\n", card_detect_start_pos);
                currentHomingState = SEARCHING_FOR_EDGE;
            }
            break;
        }
        case SEARCHING_FOR_EDGE: {
            stepper.runSpeed();
            static unsigned long lastCheck = 0;
            if(millis() - lastCheck > 50) {
              lastCheck = millis();
              if (!mfrc522.PICC_IsNewCardPresent()) {
                  card_detect_end_pos = stepper.currentPosition();
                  Serial.printf("[HOMING] Card trailing edge found at: %ld\n", card_detect_end_pos);
                  long center_pos = (card_detect_start_pos + card_detect_end_pos) / 2;
                  Serial.printf("[HOMING] Precise center calculated: %ld. Moving to center...\n", center_pos);
                  stepper.moveTo(center_pos);
                  currentHomingState = MOVING_TO_CENTER;
              }
            }
            break;
        }
        case MOVING_TO_CENTER: {
            if (stepper.distanceToGo() == 0) {
                Serial.println("[HOMING] Centered. Position is now ZERO.");
                stepper.setCurrentPosition(0);
                digitalWrite(STEPPER_ENABLE_PIN, HIGH);
                currentHomingState = HOMING_COMPLETE;
                resetAndStartVoltmeterGame();
            } else {
                stepper.run();
            }
            break;
        }
        case HOMING_COMPLETE: {
             break;
        }
    }
}

void runGame1_VoltmeterSelect() {
    unsigned long currentTime = millis();
    if (currentTime - game1_lastSelectionUpdateTime >= game1_selectionInterval) {
        game1_lastSelectionUpdateTime = currentTime;
        game1_targetVoltage += game1_voltageDirection;
        if (game1_targetVoltage >= 10.0) { game1_targetVoltage = 10.0; game1_voltageDirection = -0.1; }
        else if (game1_targetVoltage <= 0.0) { game1_targetVoltage = 0.0; game1_voltageDirection = 0.1; }
        int target_voltage_scaled = game1_targetVoltage * 100;
        game1_selectionDutyCycle = multiMap(target_voltage_scaled, calibrated_voltage_points, calibrated_pwm_points, calibration_points_count);
        ledcWrite(pwmChannel, game1_selectionDutyCycle);
    }
    if (digitalRead(GAME1_BUTTON_PIN) == LOW && (millis() - game1_lastButtonPressTime > game1_debounceDelay)) {
        game1_lastButtonPressTime = millis();
        digitalWrite(POWER_ENABLE_PIN, LOW);
        game1_selectedLevel = round(game1_targetVoltage);
        if (game1_selectedLevel < 1) game1_selectedLevel = 1;
        if (game1_selectedLevel > 10) game1_selectedLevel = 10;
        Serial.printf("[ACTION] Voltmeter stopped. Selected level: %d\n", game1_selectedLevel);
        long totalSteps = (long)(game1_selectedLevel) * STEPS_PER_360_DEG;
        Serial.printf("[MOTOR] Move calculated: %ld steps for %d full turns.\n", totalSteps, game1_selectedLevel);
        stepper.moveTo(totalSteps);
        digitalWrite(STEPPER_ENABLE_PIN, LOW);
        currentGameMode = MODE_STEPPER_MOVING;
    }
}

void runStepperMotor() {
    if (stepper.distanceToGo() == 0) {
        Serial.println("[MOTOR] Target reached. Switching to slow search mode.");
        stepper.setSpeed(stepper_search_speed);
        currentGameMode = MODE_AWAITING_AUTO_RFID;
    } else {
        stepper.run();
    }
}

void runStepperSearchAndRFID() { stepper.runSpeed(); handleCardAndStartGame(); }

void runGame2_ARGB_Microphone() {
    // Oyunu resetlemek için buton kontrolü
    if (digitalRead(GAME1_BUTTON_PIN) == LOW && (millis() - game1_lastButtonPressTime > game1_debounceDelay)) {
        game1_lastButtonPressTime = millis();
        setIdleMode(); 
        delay(100);
        currentGameMode = MODE_HOMING;
        currentHomingState = SEARCHING_FOR_CARD;
        return;
    }

    // LED parlaklığı 0'a ulaştıysa oyunu kazan
    int currentBrightness = strip.getBrightness();
    if (currentBrightness <= 0) {
        Serial.println("[GAME] Microphone Game WON!");
        currentGameMode = MODE_GAME2_WON;
        return;
    }
    
    // Parlaklık güncelleme zamanlayıcısı (artık programı bloklamıyor)
    static unsigned long lastUpdate = 0;
    if(millis() - lastUpdate > 50) { // Her 50ms'de bir kontrol et
        lastUpdate = millis();

        // Dijital mikrofon pinini oku
        bool isMicActive = (digitalRead(DIGITAL_MIC_PIN) == HIGH);
        
        // Parlaklığı ayarla
        if (isMicActive) {
            currentBrightness -= brightness_decrease_step;
        } else {
            currentBrightness += brightness_increase_step;
        }
        currentBrightness = constrain(currentBrightness, 0, 255);

        // Yeni parlaklığı LED'e ve loglara yansıt
        strip.setBrightness(currentBrightness);
        strip.show();
        Serial.printf("[MIC GAME] Mic State: %s, Brightness: %d\n", isMicActive ? "HIGH (Active)" : "LOW (Passive)", currentBrightness);
    }
}

void runGame2WinSequence() {
    static unsigned long winStartTime = 0;
    static unsigned long lastBlinkTime = 0;
    static bool ledState = true;
    static bool commandsSent = false;
    if (winStartTime == 0) {
        winStartTime = millis();
        commandsSent = false;
        Serial.println("[STATE] Game 2 Won! Announcing to ESP1 and waiting 5 seconds...");
        if (client.connected()) {
            client.publish(mqtt_status_topic, "{\"event\":\"game2_won\"}");
        }
    }
    if (millis() - lastBlinkTime > 200) {
        lastBlinkTime = millis();
        ledState = !ledState;
        if (ledState) {
            strip.setPixelColor(0, strip.Color(0, 255, 0));
            strip.setBrightness(255);
        } else {
            strip.setBrightness(0);
        }
        strip.show();
    }
    if (millis() - winStartTime > 5000 && !commandsSent) {
        Serial.println("[TRANSITION] Win sequence finished. Triggering Game 1 on ESP1.");
        if(client.connected()) {
            client.publish(topic_game_control, "{\"action\":\"start_game\"}");
        }
        commandsSent = true;
        setIdleMode();
        delay(100);
        currentGameMode = MODE_HOMING;
        currentHomingState = SEARCHING_FOR_CARD;
    }
}

void runIdleMode() {
    if (digitalRead(GAME1_BUTTON_PIN) == LOW && (millis() - game1_lastButtonPressTime > game1_debounceDelay)) {
        game1_lastButtonPressTime = millis();
        Serial.println("[ACTION] Restarting from IDLE via button press.");
        currentGameMode = MODE_HOMING;
        currentHomingState = SEARCHING_FOR_CARD;
        return;
    }
    digitalWrite(POWER_ENABLE_PIN, LOW);
    ledcWrite(pwmChannel, 0);
    strip.setBrightness(0);
    strip.show();
    digitalWrite(STEPPER_ENABLE_PIN, HIGH);
}

void startMicrophoneGame() {
    Serial.println("[ACTION] Starting Microphone Game directly.");
    strip.setBrightness(255);
    strip.setPixelColor(0, strip.Color(255, 0, 0));
    strip.show();
    currentGameMode = MODE_GAME2_ARGB_MICROPHONE;
    notifyClients();
}

void setIdleMode() {
    Serial.println("[ACTION] Setting to Idle Mode.");
    currentGameMode = MODE_IDLE;
    notifyClients();
}