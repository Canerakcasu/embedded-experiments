/**
 * @file esp32_master_controller.ino
 * @brief Game Master ESP with Web UI and Final Multi-Point Calibration.
 * @version 12.0 - Calibrated
 * @date 12 June 2025
 */

//--- KÜTÜPHANELER ---
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
#include "index_h.h" // Web arayüzünü içeren dosya (değişiklik yok)

// ====================================================================================================
//                                      KALİBRASYON VERİLERİ
// ====================================================================================================
// <<< SİZİN VERİLERİNİZ BURAYA GİRİLDİ >>>
// Voltaj değerleri * 100 olarak yazılmıştır (örn: 2.0V -> 200)
int calibrated_voltage_points[] = {  0, 200, 400, 600, 700, 800, 1000 };
int calibrated_pwm_points[]     = {  0,  27,  31,  37,  65, 117,  220 }; // Not: 220, 10V için bir tahmindir.
int calibration_points_count = sizeof(calibrated_voltage_points) / sizeof(int);


// ====================================================================================================
//                                      TÜM TANIMLAMALAR
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
#define MIC_PIN               34

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

// --- Web Sunucusu ve WebSocket Nesneleri ---
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
WiFiClient espClient;
PubSubClient client(espClient);

// --- Kontrol Edilebilir Ayarlar ---
long game1_selectionInterval = 50; // İbrenin hızını buradan ayarlayabilirsiniz
int game1_debounceDelay = 500;
int STEPS_PER_90_DEG = 50;
int stepper_max_speed = 250;
int stepper_acceleration = 100;
int stepper_search_speed = 25;
int mic_threshold_high = 1800;
int mic_threshold_low = 1500;
int brightness_decrease_step = 10;
int brightness_increase_step = 2;

// --- Genel Değişkenler ve Oyun Durumları ---
enum GameMode { MODE_IDLE = 0, MODE_MANUAL_CONTROL, MODE_GAME1_VOLTMETER_SELECT, MODE_STEPPER_MOVING, MODE_AWAITING_AUTO_RFID, MODE_GAME2_ARGB_MICROPHONE, MODE_GAME2_WON };
GameMode currentGameMode = MODE_GAME1_VOLTMETER_SELECT;
String last_rfid_uid = "None";

AccelStepper stepper(AccelStepper::DRIVER, STEPPER_STEP_PIN, STEPPER_DIR_PIN);
MFRC522 mfrc522(RFID_SS_PIN, RFID_RST_PIN);
Adafruit_NeoPixel strip(1, LED_PIN, NEO_GRB + NEO_KHZ800);

// --- PWM Ayarları ---
const int pwmFrequency = 5000;
const int pwmChannel = 0;
const int pwmResolution = 12; // PWM Çözünürlüğü 12-bit olarak ayarlandı

float game1_targetVoltage = 0.0;
float game1_voltageDirection = 0.1;
int game1_selectionDutyCycle = 0;
unsigned long game1_lastSelectionUpdateTime = 0;
unsigned long game1_lastButtonPressTime = 0;
int game1_selectedLevel = 0;

#define NUM_READINGS 50
int game2_micReadings[NUM_READINGS];
int game2_readIndex = 0; long game2_total = 0; int game2_averageMicValue = 0;
bool game2_isBlowing = false;
unsigned long game2_lastMicReadTime = 0; unsigned long game2_lastBrightnessUpdateTime = 0;

// --- FONKSİYON PROTOTİPLERİ ---
void notifyClients();
String getStatusJSON();
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void runGame1_VoltmeterSelect();
void runStepperMotor();
void runStepperSearchAndRFID();
void runGame2_ARGB_Microphone();
void runIdleMode();
void resetAndStartVoltmeterGame();
void handleCardAndStartGame();
bool compareUIDs(byte* uid1, const byte* uid2, int size);
void runGame2WinSequence();
int multiMap(int val, int* in, int* out, int size);


// ====================================================================================================
//                                          SETUP
// ====================================================================================================
void setup() {
    Serial.begin(115200);
    pinMode(POWER_ENABLE_PIN, OUTPUT);
    pinMode(GAME1_BUTTON_PIN, INPUT_PULLUP);
    pinMode(STEPPER_ENABLE_PIN, OUTPUT);
    digitalWrite(STEPPER_ENABLE_PIN, HIGH);
    digitalWrite(POWER_ENABLE_PIN, HIGH);

    SPI.begin();
    mfrc522.PCD_Init();
    strip.begin();
    strip.show();
    stepper.setMaxSpeed(stepper_max_speed);
    stepper.setAcceleration(stepper_acceleration);
    
    ledcSetup(pwmChannel, pwmFrequency, pwmResolution);
    ledcAttachPin(VOLTMETER_CONTROL_PIN, pwmChannel);

    WiFi.config(staticIP, gateway, subnet);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected. IP Address: ");
    Serial.println(WiFi.localIP());

    client.setServer(mqtt_server, 1883);

    ws.onEvent(onEvent);
    server.addHandler(&ws);
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html, nullptr);
    });
    server.begin();

    game1_lastSelectionUpdateTime = millis();
}

// ====================================================================================================
//                                          LOOP
// ====================================================================================================
void loop() {
    ws.cleanupClients();
    if (client.connected()) {
        client.loop();
    }

    switch (currentGameMode) {
        case MODE_IDLE:                   runIdleMode(); break;
        case MODE_GAME1_VOLTMETER_SELECT: runGame1_VoltmeterSelect(); break;
        case MODE_STEPPER_MOVING:         runStepperMotor(); break;
        case MODE_AWAITING_AUTO_RFID:     runStepperSearchAndRFID(); break;
        case MODE_GAME2_ARGB_MICROPHONE:  runGame2_ARGB_Microphone(); break;
        case MODE_GAME2_WON:              runGame2WinSequence(); break;
        case MODE_MANUAL_CONTROL:         
            if (stepper.distanceToGo() == 0) {
                digitalWrite(STEPPER_ENABLE_PIN, HIGH);
                currentGameMode = MODE_IDLE;
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

// ====================================================================================================
//                                      WEB ARAYÜZÜ İLETİŞİMİ
// ====================================================================================================
void notifyClients() { if(ws.count() > 0){ ws.textAll(getStatusJSON()); } }

String getStatusJSON() {
    JSONVar status;
    const char* mode_text;
    switch(currentGameMode){
        case MODE_IDLE:                   mode_text = "Idle"; break;
        case MODE_MANUAL_CONTROL:         mode_text = "Manual Control"; break;
        case MODE_GAME1_VOLTMETER_SELECT: mode_text = "Voltmeter Selection"; break;
        case MODE_STEPPER_MOVING:         mode_text = "Motor Moving to Target"; break;
        case MODE_AWAITING_AUTO_RFID:     mode_text = "Searching for RFID..."; break;
        case MODE_GAME2_ARGB_MICROPHONE:  mode_text = "Microphone Game Active"; break;
        case MODE_GAME2_WON:              mode_text = "Game 2 Won!"; break;
        default:                          mode_text = "Unknown State";
    }
    status["gameMode"] = mode_text;
    status["buttonState"] = (digitalRead(GAME1_BUTTON_PIN) == LOW) ? "Pressed" : "Released";
    status["micRaw"] = analogRead(MIC_PIN);
    status["micAvg"] = game2_averageMicValue;
    status["motorPosition"] = stepper.currentPosition();
    status["motorSpeed"] = stepper.speed();
    status["motorRunning"] = (stepper.distanceToGo() != 0);
    status["rfidStatus"] = last_rfid_uid;
    status["voltmeterSpeed"] = game1_selectionInterval;
    return JSON.stringify(status);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        JSONVar cmd = JSON.parse((char*)data);
        if (JSON.typeof(cmd) == "undefined") { return; }
        String action = (const char*)cmd["action"];
        if (action == "startGame") { resetAndStartVoltmeterGame(); } 
        else if (action == "resetSystem") { ESP.restart(); }
        else if (action == "moveMotor") {
            currentGameMode = MODE_MANUAL_CONTROL;
            digitalWrite(STEPPER_ENABLE_PIN, LOW);
            stepper.move((int)cmd["steps"]);
        }
        else if (action == "setVoltmeterSpeed") { game1_selectionInterval = (long)cmd["value"]; }
    }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if(type == WS_EVT_CONNECT){ Serial.printf("WS Client #%u connected\n", client->id()); notifyClients(); }
    else if(type == WS_EVT_DISCONNECT){ Serial.printf("WS Client #%u disconnected\n", client->id()); }
    else if(type == WS_EVT_DATA){ handleWebSocketMessage(arg, data, len); }
}

// ====================================================================================================
//                                      OYUN VE YARDIMCI FONKSİYONLAR
// ====================================================================================================

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
    Serial.println("Game sequence restarting...");
    if (!client.connected()) client.connect(mqtt_client_id);
    client.publish(topic_game_control, "{\"action\":\"reset_game\"}");
    digitalWrite(POWER_ENABLE_PIN, HIGH);
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
        int game2_currentBrightness = 255; game2_isBlowing = false;
        strip.setBrightness(game2_currentBrightness);
        strip.setPixelColor(0, strip.ColorHSV(0));
        strip.show();
        currentGameMode = MODE_GAME2_ARGB_MICROPHONE;
    } else if (compareUIDs(mfrc522.uid.uidByte, CARD_UID_GAME1, 4)) {
        if (!client.connected()) client.connect(mqtt_client_id);
        char controlMsg[20]; sprintf(controlMsg, "start_game:%d", game1_selectedLevel);
        client.publish(topic_game_control, controlMsg);
        currentGameMode = MODE_IDLE;
    } else {
        currentGameMode = MODE_IDLE;
    }
    mfrc522.PICC_HaltA();
    notifyClients();
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
        if (game1_selectedLevel < 0) game1_selectedLevel = 0;
        if (game1_selectedLevel > 10) game1_selectedLevel = 10;
        if (game1_selectedLevel == 0) game1_selectedLevel = 1;
        currentGameMode = MODE_STEPPER_MOVING;
        long totalSteps = (long)(game1_selectedLevel) * STEPS_PER_90_DEG;
        stepper.setCurrentPosition(0);
        stepper.moveTo(totalSteps);
        digitalWrite(STEPPER_ENABLE_PIN, LOW);
    }
}

void runStepperMotor() {
    if (stepper.distanceToGo() == 0) {
        stepper.setSpeed(stepper_search_speed);
        currentGameMode = MODE_AWAITING_AUTO_RFID;
    } else {
        stepper.run();
    }
}

void runStepperSearchAndRFID() { stepper.runSpeed(); handleCardAndStartGame(); }

void runGame2_ARGB_Microphone() {
    if (digitalRead(GAME1_BUTTON_PIN) == LOW && (millis() - game1_lastButtonPressTime > game1_debounceDelay)) {
        game1_lastButtonPressTime = millis();
        resetAndStartVoltmeterGame();
        return;
    }
    int currentBrightness = strip.getBrightness();
    if (currentBrightness <= 0) {
        Serial.println("!!! Mikrofon Oyunu KAZANILDI !!!");
        currentGameMode = MODE_GAME2_WON;
        return;
    }
    if (millis() - game2_lastMicReadTime > 10) {
        game2_lastMicReadTime = millis();
        int micValue = analogRead(MIC_PIN);
        game2_total = 0; for(int i=0; i<NUM_READINGS-1; i++) {game2_micReadings[i] = game2_micReadings[i+1]; game2_total += game2_micReadings[i];}
        game2_micReadings[NUM_READINGS-1] = micValue; game2_total += micValue;
        game2_averageMicValue = game2_total / NUM_READINGS;
        if (game2_averageMicValue > mic_threshold_high) { game2_isBlowing = true; }
        else if (game2_averageMicValue < mic_threshold_low) { game2_isBlowing = false; }
    }
    if (millis() - game2_lastBrightnessUpdateTime > 50) {
        game2_lastBrightnessUpdateTime = millis();
        if (game2_isBlowing) { currentBrightness -= brightness_decrease_step; }
        else { currentBrightness += brightness_increase_step; }
        if (currentBrightness < 0) currentBrightness = 0;
        if (currentBrightness > 255) currentBrightness = 255;
        strip.setBrightness(currentBrightness);
        strip.show();
    }
}

void runGame2WinSequence() {
    static unsigned long winStartTime = 0;
    static unsigned long lastBlinkTime = 0;
    static bool ledState = true;
    if (winStartTime == 0) { winStartTime = millis(); }
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
    if (millis() - winStartTime > 5000) {
        winStartTime = 0;
        currentGameMode = MODE_IDLE;
    }
}

void runIdleMode() {
    if (digitalRead(GAME1_BUTTON_PIN) == LOW && (millis() - game1_lastButtonPressTime > game1_debounceDelay)) {
        game1_lastButtonPressTime = millis();
        resetAndStartVoltmeterGame();
        return;
    }
    digitalWrite(POWER_ENABLE_PIN, LOW);
    ledcWrite(pwmChannel, 0);
    strip.setBrightness(0);
    strip.show();
    digitalWrite(STEPPER_ENABLE_PIN, HIGH);
}