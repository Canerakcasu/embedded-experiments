#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <HardwareSerial.h>
#include "DFRobotDFPlayerMini.h"
#include <IRremote.h>
#include <AiEsp32RotaryEncoder.h>
#include <Preferences.h>
#include <PubSubClient.h>

const char* ssid = "YOUR_WIFI_ID";
const char* password = "PASSWORD";

const char* mqtt_server = "192.168.20.208";
const int mqtt_port = 1883;
const char* mqtt_client_id = "ESP32_ControlHub_GM_IRSim";

WiFiClient espClient;
PubSubClient mqttClient(espClient);
unsigned long lastMqttPublishTime = 0;
const long mqttPublishInterval = 3000; 

WebServer server(80);

#define LED_PIN         2
#define NUM_LEDS        15
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
int globalBrightness = 120;
uint8_t currentLedR = 255, currentLedG = 0, currentLedB = 0;
bool ledsOn = true;

#define ROTARY_ENCODER_A_PIN    33
#define ROTARY_ENCODER_B_PIN    27
#define ROTARY_ENCODER_BUTTON_PIN 14
AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, -1, 4);
int currentEncoderMode = 0; 
const char* encoderModes[] = {"LED Brightness", "LED Color Select", "DFPlayer Volume"};
uint32_t predefinedColors[] = {
    strip.Color(255, 0, 0), strip.Color(0, 255, 0), strip.Color(0, 0, 255),   
    strip.Color(255, 255, 0), strip.Color(0, 255, 255), strip.Color(255, 0, 255), 
    strip.Color(255, 255, 255) 
};
const int num_predefined_colors = sizeof(predefinedColors) / sizeof(predefinedColors[0]);
int currentPredefinedColorIndex = 0;

HardwareSerial myDFPlayerSerial(2);
DFRobotDFPlayerMini myDFPlayer;
bool dfPlayerAvailable = false;
bool sdCardOnline = false;
int currentDFPlayerVolume = 20;
int previousDFPlayerVolume = 20; 
bool dfPlayerMuted = false;
int currentTrackNumber = 0;
const int totalTracks = 5; 
const char* trackNames[totalTracks + 1] = {
    "Unknown/Stopped", "Track 1 (e.g. Intro)", "Track 2 (e.g. Birds)", 
    "Track 3 (e.g. Melody)", "Track 4 (e.g. Effect)", "Track 5 (e.g. Ambiance)"
};

#define DFPLAYER_RX_PIN 16
#define DFPLAYER_TX_PIN 17

#define EXTERNAL_BUTTON_D12_PIN 12

#define IR_RECEIVE_PIN 18 
int irDpadMode = 0; 
const char* irDpadModes[] = {"LED D-Pad Control", "DFPlayer D-Pad Control"};

// IR Action Definitions
const char* IR_ACTION_MUTE = "mute";
const char* IR_ACTION_POWER = "power";
const char* IR_ACTION_MUSIC = "music_btn";
const char* IR_ACTION_PLAYSTOP = "play_stop";
const char* IR_ACTION_UP = "up";
const char* IR_ACTION_RIGHT = "right";
const char* IR_ACTION_LEFT = "left";
const char* IR_ACTION_DOWN = "down";
const char* IR_ACTION_ENTER = "enter";
const char* IR_ACTION_VOL_UP = "vol_up";
const char* IR_ACTION_VOL_DOWN = "vol_down";
const char* IR_ACTION_FFWD = "ffwd";
const char* IR_ACTION_REW = "rew";
const char* IR_ACTION_NEXT_TRACK = "next_track";
const char* IR_ACTION_BACK_TRACK = "back_track";


StaticJsonDocument<1024> jsonDocHttp;
StaticJsonDocument<512> jsonDocMqtt; 

const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><title>ESP32 Advanced Control</title><meta name="viewport" content="width=device-width, initial-scale=1"><style>body{font-family:'Roboto',Arial,sans-serif;background-color:#121212;color:#e0e0e0;padding:15px;display:flex;flex-direction:column;align-items:center;margin:0}h1{color:#e53935;text-align:center;text-shadow:1px 1px 3px #000;margin-bottom:25px}h3{color:#ff7961;text-align:left;border-bottom:2px solid #b71c1c;padding-bottom:8px;margin-top:20px;margin-bottom:15px}.grid-container{display:grid;grid-template-columns:repeat(auto-fit, minmax(320px, 1fr));gap:25px;width:100%;max-width:1200px}.card{background-color:#1e1e1e;border:1px solid #c21807;border-radius:10px;padding:20px;box-shadow:0 6px 12px rgba(0,0,0,0.5)}button{background-color:#b71c1c;color:white;border:none;padding:10px 15px;margin:5px;border-radius:6px;cursor:pointer;font-size:1em;font-weight:500;transition:background-color .3s,transform .1s;box-shadow:0 2px 4px rgba(0,0,0,0.3)}button:hover{background-color:#f4511e;transform:translateY(-1px)}.status-item{margin-bottom:12px;font-size:1em;line-height:1.6}.status-label{font-weight:700;color:#ffab91}.track-buttons button,.ir-sim-buttons button{min-width:48px;height:48px;margin:4px}.color-button{width:38px;height:38px;margin:5px;border:2px solid #121212;border-radius:50%;padding:0;cursor:pointer}.color-preview{width:50px;height:25px;border:2px solid #ffab91;display:inline-block;vertical-align:middle;margin-left:10px;border-radius:4px}input[type=color]{width:50px;height:35px;border:1px solid #555;border-radius:4px;padding:2px;cursor:pointer;vertical-align:middle;margin-left:10px}input[type=range]{width:calc(100% - 180px);margin:0 10px;vertical-align:middle;cursor:pointer}label{vertical-align:middle;margin-right:5px}.controls-row{display:flex;flex-wrap:wrap;align-items:center;margin-bottom:12px;gap:10px}</style></head><body><h1>ESP32 Advanced Control Panel</h1><div class="grid-container"><div class="card"><h3>General Status</h3><div class="status-item"><span class="status-label">WiFi Network: </span><span id="wifiSSID">Loading...</span></div><div class="status-item"><span class="status-label">MQTT Status: </span><span id="mqttStatus">Loading...</span></div><div class="status-item"><span class="status-label">Active Encoder Mode: </span><span id="encoderModeStatus">Loading...</span></div><div class="status-item"><span class="status-label">Encoder Button (GPIO14): </span><span id="encoderButtonStatus">Not Pressed</span></div><div class="status-item"><span class="status-label">External Button (GPIO12): </span><span id="externalButtonD12Status">Not Pressed</span></div><div class="status-item"><span class="status-label">IR D-Pad Mode: </span><span id="irDpadModeStatus">LED Control</span></div><div class="status-item"><span class="status-label">SD Card: </span><span id="sdStatus">Loading...</span></div></div><div class="card"><h3>LED Control</h3><div class="controls-row"><button id="ledToggleBtn" onclick="sendIrAction('power')">Toggle LEDs (IR Power)</button></div><div class="controls-row"><label for="brightnessSlider" class="status-label">Brightness:</label><input type="range" id="brightnessSlider" min="0" max="255" value="120" oninput="updateBrightnessValue(this.value)" onchange="setBrightness(this.value)"><span id="brightnessValue">120</span></div><div class="status-item controls-row"><span class="status-label">Current Color:</span><input type="color" id="rgbColorPicker" onchange="setRGBColor(this.value)"><div id="currentColorPreview" class="color-preview"></div></div></div><div class="card"><h3>DFPlayer Control</h3><div class="status-item"><span class="status-label">Volume Level: </span><span id="volumeStatus">Loading...</span> (<span id="dfPlayerMutedStatus"></span>)</div><div class="status-item"><span class="status-label">Playing Track: </span><span id="trackStatus">Loading...</span> (<span id="trackNameStatus"></span>)</div><div class="controls-row"><label for="dfVolumeSlider" class="status-label">Set Volume:</label><input type="range" id="dfVolumeSlider" min="0" max="30" value="20" oninput="updateDfVolumeValue(this.value)" onchange="setDFVolume(this.value)"><span id="dfVolumeValue">20</span></div><div class="controls-row button-group"><button onclick="sendIrAction('play_stop')">Play/Pause (IR)</button><button onclick="sendIrAction('next_track')">Next (IR)</button><button onclick="sendIrAction('back_track')">Prev (IR)</button><button onclick="sendIrAction('mute')">Mute/Unmute (IR)</button></div><div><p class="status-label">Select Track (Direct):</p><span id="trackButtonsContainer"></span></div></div><div class="card"><h3>Simulate IR Remote</h3><div class="ir-sim-buttons"><button onclick="sendIrAction('power')">Power</button><button onclick="sendIrAction('mute')">Mute</button><button onclick="sendIrAction('music_btn')">Music</button><button onclick="sendIrAction('play_stop')">Play/Stop</button><br><button onclick="sendIrAction('up')">Up</button><button onclick="sendIrAction('down')">Down</button><button onclick="sendIrAction('left')">Left</button><button onclick="sendIrAction('right')">Right</button><button onclick="sendIrAction('enter')">Enter</button><br><button onclick="sendIrAction('vol_up')">Vol+</button><button onclick="sendIrAction('vol_down')">Vol-</button><button onclick="sendIrAction('ffwd')">FFWD</button><button onclick="sendIrAction('rew')">REW</button><button onclick="sendIrAction('next_track')">Next Trk</button><button onclick="sendIrAction('back_track')">Back Trk</button></div></div></div><script>const totalTracks=5;function sendCommand(url){fetch(url).then(response=>{if(!response.ok)console.error('Command Error:',response.status);return response.text()}).then(data=>console.log(url+'->'+data)).catch(error=>console.error('Fetch Error:',error));setTimeout(updateStatus,300)}function sendIrAction(actionName){sendCommand('/ir_action?cmd='+actionName)}function setBrightness(value){sendCommand('/set_brightness?value='+value)}function updateBrightnessValue(value){document.getElementById('brightnessValue').textContent=value}function setRGBColor(hexColor){const r=parseInt(hexColor.substr(1,2),16);const g=parseInt(hexColor.substr(3,2),16);const b=parseInt(hexColor.substr(5,2),16);sendCommand(`/set_led_rgb?r=${r}&g=${g}&b=${b}`)}function setDFVolume(volume){sendCommand('/set_df_volume?value='+volume)}function updateDfVolumeValue(value){document.getElementById('dfVolumeValue').textContent=value}function playTrack(trackNum){sendCommand('/play_track?track='+trackNum)}function updateStatus(){fetch('/status').then(response=>response.json()).then(data=>{document.getElementById('wifiSSID').textContent=data.wifiSSID||"Not Connected";document.getElementById('mqttStatus').textContent=data.mqttConnected?"Connected":"Disconnected";document.getElementById('encoderModeStatus').textContent=data.encoderMode;document.getElementById('encoderButtonStatus').textContent=data.encoderButtonPressed?"Pressed":"Not Pressed";document.getElementById('externalButtonD12Status').textContent=data.externalButtonD12Pressed?"Pressed":"Not Pressed";document.getElementById('irDpadModeStatus').textContent=data.irDpadMode;document.getElementById('sdStatus').textContent=data.sdCardOnline?"Online":"Offline/Error";document.getElementById('ledToggleBtn').textContent=data.ledsOn?"Turn LEDs Off":"Turn LEDs On";const brightnessSlider=document.getElementById('brightnessSlider');if(document.activeElement!==brightnessSlider){brightnessSlider.value=data.brightness}document.getElementById('brightnessValue').textContent=data.brightness;const colorHex='#'+[data.ledR,data.ledG,data.ledB].map(c=>parseInt(c).toString(16).padStart(2,'0')).join('');document.getElementById('currentColorPreview').style.backgroundColor=colorHex;const colorPicker=document.getElementById('rgbColorPicker');if(document.activeElement!==colorPicker)colorPicker.value=colorHex;document.getElementById('volumeStatus').textContent=data.dfVolume!==-1?data.dfVolume:"N/A";document.getElementById('dfPlayerMutedStatus').textContent=data.dfPlayerMuted?"MUTED":"";const dfVolumeSlider=document.getElementById('dfVolumeSlider');if(document.activeElement!==dfVolumeSlider){dfVolumeSlider.value=data.dfVolume!==-1?data.dfVolume:20}document.getElementById('dfVolumeValue').textContent=data.dfVolume!==-1?data.dfVolume:20;document.getElementById('trackStatus').textContent=data.dfTrack!==-1&&data.dfTrack!==0?data.dfTrack:"Stopped";document.getElementById('trackNameStatus').textContent=data.dfTrackName||"";console.log("Status updated:",data)}).catch(error=>console.error('Could not get status:',error))}function createTrackButtons(){const container=document.getElementById('trackButtonsContainer');if(container){container.innerHTML='';for(let i=1;i<=totalTracks;i++){const btn=document.createElement('button');btn.textContent=i;btn.onclick=function(){playTrack(i)};container.appendChild(btn)}}}window.onload=()=>{createTrackButtons();updateStatus();setInterval(updateStatus,2200)};</script></body></html>
)rawliteral";

// Function Prototypes
void setupHardware();
void setupWiFi();
void setupMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void reconnectMQTT();
void publishStatusMQTT(bool forcePublish = false);
void setupWebServer();
void handleRotaryEncoder();
void updateLeds();
void handleRoot();
void handleStatus();
void handleToggleLed();
void handleSetBrightness();
void handleSetLedRGB();
void handleSetDFVolume();
void handlePlayTrack();
void handleDFCommand(const char* cmd);
void handleDFMuteToggle();
void handleIrActionFromWeb(); // New handler for web IR simulation
void handleNotFound();
void processIRCode(uint32_t irCode);
uint32_t getHexForIrAction(String actionName);


void setup() {
    Serial.begin(115200);
    Serial.println("\n\n--- ESP32 Advanced Control Panel (IR Sim + MQTT) ---");
    setupHardware();
    setupWiFi();
    setupMQTT(); 
    setupWebServer();
    Serial.println("Setup Complete. IP Address:");
    Serial.println(WiFi.localIP());
    Serial.println("MQTT Broker: " + String(mqtt_server));
    Serial.println("IR D-Pad Mode: " + String(irDpadModes[irDpadMode]));
    Serial.println("------------------------------------");
}

void loop() {
    if (WiFi.status() == WL_CONNECTED) {
        if (!mqttClient.connected()) {
            reconnectMQTT();
        }
        mqttClient.loop(); 
    }
    server.handleClient(); 
    handleRotaryEncoder();

    if (IrReceiver.decode()) {
        processIRCode(IrReceiver.decodedIRData.decodedRawData);
        IrReceiver.resume();
    }

    bool dfStatusChanged = false;
    if (dfPlayerAvailable && myDFPlayer.available()) {
        uint8_t type = myDFPlayer.readType();
        int value = myDFPlayer.read();
        switch (type) {
            case DFPlayerPlayFinished:
                Serial.print("Track Finished: "); Serial.println(value);
                if (currentTrackNumber != 0) dfStatusChanged = true;
                currentTrackNumber = 0; 
                break;
            case DFPlayerError:
                Serial.print("DFPlayer Error, Code: "); Serial.println(value);
                dfStatusChanged = true;
                break;
            case DFPlayerCardOnline:
                if (!sdCardOnline) dfStatusChanged = true;
                sdCardOnline = true;
                break;
            case DFPlayerCardRemoved:
                if (sdCardOnline) dfStatusChanged = true;
                sdCardOnline = false;
                break;
        }
    }

    if (millis() - lastMqttPublishTime > mqttPublishInterval) {
        publishStatusMQTT(true);
    } else if (dfStatusChanged) {
        publishStatusMQTT();
    }
}

void setupHardware() {
    strip.begin();
    strip.setBrightness(globalBrightness);
    currentLedR = predefinedColors[currentPredefinedColorIndex] >> 16 & 0xFF;
    currentLedG = predefinedColors[currentPredefinedColorIndex] >> 8 & 0xFF;
    currentLedB = predefinedColors[currentPredefinedColorIndex] & 0xFF;
    updateLeds();
    Serial.println("NeoPixel Initialized. Pin: " + String(LED_PIN));

    rotaryEncoder.begin();
    rotaryEncoder.setup([] { rotaryEncoder.readEncoder_ISR(); });
    rotaryEncoder.setBoundaries(0, 255, false); 
    rotaryEncoder.setEncoderValue(globalBrightness);
    Serial.println("Rotary Encoder Initialized. CLK:" + String(ROTARY_ENCODER_A_PIN) + " DT:" + String(ROTARY_ENCODER_B_PIN) + " SW:" + String(ROTARY_ENCODER_BUTTON_PIN) + ". Mode: " + encoderModes[currentEncoderMode]);

    pinMode(EXTERNAL_BUTTON_D12_PIN, INPUT_PULLUP);
    Serial.println("External D12 Button Initialized. Pin: " + String(EXTERNAL_BUTTON_D12_PIN));

    myDFPlayerSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
    Serial.println("DFPlayer Serial Port Initialized.");
    if (!myDFPlayer.begin(myDFPlayerSerial, false, false)) {
        Serial.println("!!! DFPlayer FAILED to initialize !!!");
        dfPlayerAvailable = false;
        sdCardOnline = false;
    } else {
        Serial.println("DFPlayer Mini online.");
        dfPlayerAvailable = true;
        sdCardOnline = true; 
        myDFPlayer.volume(currentDFPlayerVolume);
        previousDFPlayerVolume = currentDFPlayerVolume; 
    }

    IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK); // IR Alıcıyı başlat
    Serial.println("IR Receiver Initialized. Pin: " + String(IR_RECEIVE_PIN));
}

void setupWiFi() {
    Serial.print("Connecting to WiFi: "); Serial.println(ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    int wifi_attempts = 0;
    while (WiFi.status() != WL_CONNECTED && wifi_attempts < 20) {
        delay(500); Serial.print(".");
        wifi_attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi Connected!");
    } else {
        Serial.println("\nWiFi Connection Failed.");
    }
}

void setupMQTT() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("MQTT not started due to WiFi connection failure.");
        return;
    }
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback(mqttCallback);
    Serial.println("MQTT Setup Done. Attempting to connect...");
    reconnectMQTT();
}

uint32_t getHexForIrAction(String actionName) {
    if (actionName == IR_ACTION_MUTE) return 0xED127F80;
    if (actionName == IR_ACTION_POWER) return 0xE11E7F80;
    if (actionName == IR_ACTION_MUSIC) return 0xFD027F80;
    if (actionName == IR_ACTION_PLAYSTOP) return 0xFB047F80;
    if (actionName == IR_ACTION_UP) return 0xFA057F80;
    if (actionName == IR_ACTION_RIGHT) return 0xF6097F80;
    if (actionName == IR_ACTION_LEFT) return 0xF8077F80;
    if (actionName == IR_ACTION_DOWN) return 0xE41B7F80;
    if (actionName == IR_ACTION_ENTER) return 0xF7087F80;
    if (actionName == IR_ACTION_VOL_UP) return 0xF30C7F80;
    if (actionName == IR_ACTION_VOL_DOWN) return 0xFF007F80;
    if (actionName == IR_ACTION_FFWD) return 0xF00F7F80;
    if (actionName == IR_ACTION_REW) return 0xF20D7F80;
    if (actionName == IR_ACTION_NEXT_TRACK) return 0xF10E7F80;
    if (actionName == IR_ACTION_BACK_TRACK) return 0xE6197F80;
    return 0; 
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.print("MQTT Message arrived ["); Serial.print(topic); Serial.print("] ");
    char msg[length + 1];
    for (unsigned int i = 0; i < length; i++) { msg[i] = (char)payload[i]; }
    msg[length] = '\0';
    Serial.println(msg);
    String topicStr = String(topic);
    bool publishUpdate = true;

    if (topicStr == "esp32/command/led/state") {
        if (strcmp(msg, "ON") == 0) ledsOn = true; else if (strcmp(msg, "OFF") == 0) ledsOn = false;
        updateLeds();
    } else if (topicStr == "esp32/command/led/brightness") {
        globalBrightness = atoi(msg); updateLeds();
    } else if (topicStr == "esp32/command/led/colorRGB") {
        int r, g, b;
        if (sscanf(msg, "%d,%d,%d", &r, &g, &b) == 3) {
            currentLedR = r; currentLedG = g; currentLedB = b;
            if (!ledsOn && (currentLedR > 0 || currentLedG > 0 || currentLedB > 0) ) ledsOn = true;
            updateLeds();
        } else { Serial.println("MQTT: Failed to parse RGB color string."); publishUpdate = false;}
    } else if (topicStr == "esp32/command/dfplayer/volume") {
        if (dfPlayerAvailable) { currentDFPlayerVolume = atoi(msg); myDFPlayer.volume(currentDFPlayerVolume); dfPlayerMuted = false; } 
        else { publishUpdate = false; }
    } else if (topicStr == "esp32/command/dfplayer/playTrack") {
        if (dfPlayerAvailable) {
            int track = atoi(msg);
            if (track >= 1 && track <= totalTracks) { myDFPlayer.play(track); currentTrackNumber = track; dfPlayerMuted = false; } 
            else { publishUpdate = false; }
        } else { publishUpdate = false; }
    } else if (topicStr == "esp32/command/dfplayer/action") {
        if (dfPlayerAvailable) {
            if (strcmp(msg, "PLAY_CURRENT") == 0) {
                 if (currentTrackNumber <= 0 || currentTrackNumber > totalTracks) {myDFPlayer.play(1); currentTrackNumber = 1;} else myDFPlayer.start();
                 dfPlayerMuted = false;
            } else if (strcmp(msg, "PAUSE") == 0) { myDFPlayer.pause(); Serial.println("MQTT: DFPlayer Paused");}
            else if (strcmp(msg, "NEXT") == 0) { myDFPlayer.next(); currentTrackNumber = 0; dfPlayerMuted = false; Serial.println("MQTT: DFPlayer Next");}
            else if (strcmp(msg, "PREV") == 0) { myDFPlayer.previous(); currentTrackNumber = 0; dfPlayerMuted = false; Serial.println("MQTT: DFPlayer Previous");}
            else if (strcmp(msg, "MUTE_TOGGLE") == 0) { // Mute toggle for MQTT
                if (dfPlayerMuted) { myDFPlayer.volume(previousDFPlayerVolume); currentDFPlayerVolume = previousDFPlayerVolume; dfPlayerMuted = false; } 
                else { previousDFPlayerVolume = myDFPlayer.readVolume(); if(previousDFPlayerVolume == -1 || previousDFPlayerVolume == 0) previousDFPlayerVolume = 20; myDFPlayer.volume(0); currentDFPlayerVolume = 0; dfPlayerMuted = true; }
            } else { Serial.print("MQTT: Unknown DFPlayer action: "); Serial.println(msg); publishUpdate = false; }
        } else { publishUpdate = false; }
    } else if (topicStr == "esp32/command/ir_action") { // MQTT IR Simulation
        String cmdName = String(msg);
        uint32_t hexCode = getHexForIrAction(cmdName);
        if (hexCode != 0) {
            Serial.print("MQTT: Simulating IR command: "); Serial.println(cmdName);
            processIRCode(hexCode);
        } else { Serial.print("MQTT: Unknown IR Sim action: "); Serial.println(cmdName); publishUpdate = false;}
    } else {
        publishUpdate = false;
    }
    if (publishUpdate) publishStatusMQTT(); 
}

void reconnectMQTT() { /* Same as before */ 
    long now = millis(); static long lastReconnectAttempt = 0;
    if (now - lastReconnectAttempt > 5000) { 
        lastReconnectAttempt = now;
        if (!mqttClient.connected() && WiFi.status() == WL_CONNECTED) {
            Serial.print("Attempting MQTT connection...");
            if (mqttClient.connect(mqtt_client_id)) {
                Serial.println("connected");
                mqttClient.subscribe("esp32/command/led/state");
                mqttClient.subscribe("esp32/command/led/brightness");
                mqttClient.subscribe("esp32/command/led/colorRGB");
                mqttClient.subscribe("esp32/command/dfplayer/volume");
                mqttClient.subscribe("esp32/command/dfplayer/playTrack");
                mqttClient.subscribe("esp32/command/dfplayer/action");
                mqttClient.subscribe("esp32/command/ir_action"); // Subscribe to new IR sim topic
                publishStatusMQTT(true); 
            } else {
                Serial.print("failed, rc="); Serial.print(mqttClient.state()); Serial.println(" try again in 5 seconds");
            }
        }
    }
}

void publishStatusMQTT(bool forcePublish) { /* Same as before, but ensure irDpadMode and dfPlayerMuted are published */
    static unsigned long lastForcedPublish = 0;
    if (forcePublish) { lastForcedPublish = millis(); } 
    else { if (millis() - lastForcedPublish < 500) return; }
    if (!mqttClient.connected() || WiFi.status() != WL_CONNECTED) return;
    char buffer[64]; 
    mqttClient.publish("esp32/status/wifiSSID", WiFi.SSID().c_str(), true);
    mqttClient.publish("esp32/status/encoderMode", encoderModes[currentEncoderMode], true);
    mqttClient.publish("esp32/status/encoderButton", (digitalRead(ROTARY_ENCODER_BUTTON_PIN) == LOW) ? "PRESSED" : "NOT_PRESSED", true);
    mqttClient.publish("esp32/status/buttonD12", (digitalRead(EXTERNAL_BUTTON_D12_PIN) == LOW) ? "PRESSED" : "NOT_PRESSED", true);
    mqttClient.publish("esp32/status/irDpadMode", irDpadModes[irDpadMode], true);
    mqttClient.publish("esp32/status/sdCard", sdCardOnline ? "ONLINE" : "OFFLINE", true);
    mqttClient.publish("esp32/status/ledsOn", ledsOn ? "ON" : "OFF", true);
    sprintf(buffer, "%d", globalBrightness); mqttClient.publish("esp32/status/brightness", buffer, true);
    sprintf(buffer, "%d,%d,%d", currentLedR, currentLedG, currentLedB); mqttClient.publish("esp32/status/led/colorRGB", buffer, true);
    mqttClient.publish("esp32/status/dfplayer/muted", dfPlayerMuted ? "MUTED" : "UNMUTED", true);
    if (dfPlayerAvailable) {
        int vol = myDFPlayer.readVolume(); sprintf(buffer, "%d", vol); mqttClient.publish("esp32/status/dfVolume", buffer, true);
        int track = myDFPlayer.readCurrentFileNumber(); sprintf(buffer, "%d", track); mqttClient.publish("esp32/status/dfTrackNumber", buffer, true);
        if (track > 0 && track <= totalTracks) { mqttClient.publish("esp32/status/dfTrackName", trackNames[track], true); } 
        else { mqttClient.publish("esp32/status/dfTrackName", trackNames[0], true); }
    } else {
        mqttClient.publish("esp32/status/dfVolume", "-1", true); mqttClient.publish("esp32/status/dfTrackNumber", "-1", true); mqttClient.publish("esp32/status/dfTrackName", trackNames[0], true);
    }
    Serial.println("MQTT Status Published."); lastMqttPublishTime = millis();
}

void setupWebServer() { /* Add /ir_action endpoint */
    if (WiFi.status() != WL_CONNECTED) { Serial.println("Web Server not started."); return; }
    server.on("/", HTTP_GET, handleRoot);
    server.on("/status", HTTP_GET, handleStatus);
    server.on("/toggle_led", HTTP_GET, handleToggleLed);
    server.on("/set_brightness", HTTP_GET, handleSetBrightness);
    server.on("/set_led_rgb", HTTP_GET, handleSetLedRGB);
    server.on("/set_df_volume", HTTP_GET, handleSetDFVolume);
    server.on("/play_track", HTTP_GET, handlePlayTrack);
    server.on("/df_play_current", HTTP_GET, [](){ handleDFCommand("play_current"); });
    server.on("/df_pause", HTTP_GET, [](){ handleDFCommand("pause"); });
    server.on("/df_next", HTTP_GET, [](){ handleDFCommand("next"); });
    server.on("/df_prev", HTTP_GET, [](){ handleDFCommand("prev"); });
    server.on("/df_mute_toggle", HTTP_GET, handleDFMuteToggle);
    server.on("/ir_action", HTTP_GET, handleIrActionFromWeb); // Yeni endpoint
    server.onNotFound(handleNotFound);
    server.begin();
    Serial.println("HTTP Server Started.");
}

void updateLeds() { /* Same as before */
    if (ledsOn) { strip.setBrightness(globalBrightness); strip.fill(strip.Color(currentLedR, currentLedG, currentLedB)); } 
    else { strip.fill(strip.Color(0,0,0)); }
    strip.show();
}

void handleRotaryEncoder() { /* Same as before (with the case 1 fix) */
    bool stateChanged = false;
    if (rotaryEncoder.encoderChanged()) {
        int value = rotaryEncoder.readEncoder(); stateChanged = true;
        switch (currentEncoderMode) {
            case 0: globalBrightness = value; updateLeds(); break;
            case 1: { currentPredefinedColorIndex = value; uint32_t newColor = predefinedColors[currentPredefinedColorIndex]; currentLedR = (newColor >> 16) & 0xFF; currentLedG = (newColor >> 8) & 0xFF; currentLedB = newColor & 0xFF; updateLeds(); break; }
            case 2: currentDFPlayerVolume = value; if (dfPlayerAvailable && !dfPlayerMuted) myDFPlayer.volume(currentDFPlayerVolume); break;
        }
    }
    if (rotaryEncoder.isEncoderButtonClicked()) {
        currentEncoderMode = (currentEncoderMode + 1) % 3; stateChanged = true;
        Serial.print("Encoder Mode Changed to: "); Serial.println(encoderModes[currentEncoderMode]);
        switch (currentEncoderMode) {
            case 0: rotaryEncoder.setBoundaries(0, 255, false); rotaryEncoder.setEncoderValue(globalBrightness); break;
            case 1: rotaryEncoder.setBoundaries(0, num_predefined_colors - 1, true); rotaryEncoder.setEncoderValue(currentPredefinedColorIndex); break;
            case 2: rotaryEncoder.setBoundaries(0, 30, false); int volToSet = currentDFPlayerVolume; if(dfPlayerAvailable && !dfPlayerMuted) { int volRead = myDFPlayer.readVolume(); volToSet = (volRead == -1 ? currentDFPlayerVolume : volRead); } else if (dfPlayerMuted) { volToSet = 0; } else { volToSet = 20; } rotaryEncoder.setEncoderValue(volToSet); break;
        }
        delay(100);
    }
    if (stateChanged && mqttClient.connected()) { publishStatusMQTT(); }
}

void processIRCode(uint32_t irCode) {
    if (irCode == 0 || irCode == 0xFFFFFFFF) return;
    Serial.printf("IR Code Received: 0x%X\n", irCode);
    bool stateChanged = true; 

    switch (irCode) {
        case 0xED127F80: 
            Serial.println("IR: Mute Toggle"); 
            if (dfPlayerAvailable) { 
                if (dfPlayerMuted) { 
                    myDFPlayer.volume(previousDFPlayerVolume); 
                    currentDFPlayerVolume = previousDFPlayerVolume; 
                    dfPlayerMuted = false; 
                } else { 
                    previousDFPlayerVolume = myDFPlayer.readVolume(); 
                    if(previousDFPlayerVolume == -1 || previousDFPlayerVolume == 0) previousDFPlayerVolume = currentDFPlayerVolume > 0 ? currentDFPlayerVolume : 15; 
                    myDFPlayer.volume(0); 
                    currentDFPlayerVolume = 0; 
                    dfPlayerMuted = true; 
                } 
            } 
            break;
        case 0xE11E7F80: 
            Serial.println("IR: LED Toggle"); 
            ledsOn = !ledsOn; 
            updateLeds(); 
            break;
        case 0xFD027F80: 
            Serial.println("IR: Play Track 1"); 
            if (dfPlayerAvailable) { 
                myDFPlayer.play(1); 
                currentTrackNumber = 1; 
                dfPlayerMuted = false;
            } 
            break;
        case 0xFB047F80: 
            Serial.println("IR: Play/Pause Toggle"); 
            if (dfPlayerAvailable) { 
                if (myDFPlayer.readState() == 1) { // 1: Playing state
                    myDFPlayer.pause();
                } else { 
                    if (currentTrackNumber <= 0 || currentTrackNumber > totalTracks) {
                        myDFPlayer.play(1); 
                        currentTrackNumber = 1;
                    } else {
                        myDFPlayer.start();
                    } 
                    dfPlayerMuted = false;
                } 
            } // This was the extra closing brace causing the issue --> } 
            break; // This break was outside the case due to the extra brace above. Now it's correct.
        case 0xF30C7F80: 
            Serial.println("IR: Volume Up"); 
            if (dfPlayerAvailable) { 
                myDFPlayer.volumeUp(); 
                currentDFPlayerVolume = myDFPlayer.readVolume(); 
                dfPlayerMuted = false; 
            } 
            break;
        case 0xFF007F80: 
            Serial.println("IR: Volume Down"); 
            if (dfPlayerAvailable) { 
                myDFPlayer.volumeDown(); 
                currentDFPlayerVolume = myDFPlayer.readVolume(); 
                dfPlayerMuted = (currentDFPlayerVolume == 0); 
            } 
            break;
        case 0xF00F7F80: 
        case 0xF10E7F80: 
            Serial.println("IR: Next Track"); 
            if (dfPlayerAvailable) { 
                myDFPlayer.next(); 
                currentTrackNumber = 0; 
                dfPlayerMuted = false; 
            } 
            break;
        case 0xF20D7F80: 
        case 0xE6197F80: 
            Serial.println("IR: Previous Track"); 
            if (dfPlayerAvailable) { 
                myDFPlayer.previous(); 
                currentTrackNumber = 0; 
                dfPlayerMuted = false; 
            } 
            break;
        case 0xF7087F80: 
            irDpadMode = (irDpadMode + 1) % 2; 
            Serial.print("IR: D-Pad Mode Changed to: "); Serial.println(irDpadModes[irDpadMode]); 
            break;
        case 0xFA057F80: 
            Serial.println("IR: Up Arrow"); 
            if (irDpadMode == 0) { 
                globalBrightness = min(255, globalBrightness + 15); 
                updateLeds(); 
            } else { 
                if (dfPlayerAvailable) { 
                    myDFPlayer.volumeUp(); 
                    currentDFPlayerVolume = myDFPlayer.readVolume(); 
                    dfPlayerMuted = false;
                } 
            } 
            break;
        case 0xE41B7F80: 
            Serial.println("IR: Down Arrow"); 
            if (irDpadMode == 0) { 
                globalBrightness = max(0, globalBrightness - 15); 
                updateLeds(); 
            } else { 
                if (dfPlayerAvailable) { 
                    myDFPlayer.volumeDown(); 
                    currentDFPlayerVolume = myDFPlayer.readVolume(); 
                    dfPlayerMuted = (currentDFPlayerVolume == 0); 
                } 
            } 
            break;
        case 0xF8077F80: 
            Serial.println("IR: Left Arrow"); 
            if (irDpadMode == 0) { 
                currentPredefinedColorIndex = (currentPredefinedColorIndex - 1 + num_predefined_colors) % num_predefined_colors; 
                uint32_t nCL = predefinedColors[currentPredefinedColorIndex]; 
                currentLedR=(nCL>>16)&0xFF; currentLedG=(nCL>>8)&0xFF; currentLedB=nCL&0xFF; 
                updateLeds(); 
            } else { 
                if (dfPlayerAvailable) { 
                    myDFPlayer.previous(); 
                    currentTrackNumber = 0; 
                    dfPlayerMuted = false;
                } 
            } 
            break;
        case 0xF6097F80: 
            Serial.println("IR: Right Arrow"); 
            if (irDpadMode == 0) { 
                currentPredefinedColorIndex = (currentPredefinedColorIndex + 1) % num_predefined_colors; 
                uint32_t nCR = predefinedColors[currentPredefinedColorIndex]; 
                currentLedR=(nCR>>16)&0xFF; currentLedG=(nCR>>8)&0xFF; currentLedB=nCR&0xFF; 
                updateLeds(); 
            } else { 
                if (dfPlayerAvailable) { 
                    myDFPlayer.next(); 
                    currentTrackNumber = 0; 
                    dfPlayerMuted = false;
                } 
            } 
            break;
        default: 
            Serial.println("IR: Unknown or Unassigned Code"); 
            stateChanged = false; 
            break;
    }

    if (stateChanged && mqttClient.connected()) { 
        publishStatusMQTT(); 
    }
}

// --- Web Handlers ---
void handleRoot() { /* Same */ server.send_P(200, "text/html", INDEX_HTML); }
void handleStatus() { /* Add irDpadMode and dfPlayerMuted to JSON */
    jsonDocHttp.clear();
    jsonDocHttp["wifiSSID"] = (WiFi.status() == WL_CONNECTED) ? WiFi.SSID() : "Not Connected";
    jsonDocHttp["mqttConnected"] = mqttClient.connected();
    jsonDocHttp["encoderMode"] = encoderModes[currentEncoderMode];
    jsonDocHttp["encoderButtonPressed"] = (digitalRead(ROTARY_ENCODER_BUTTON_PIN) == LOW);
    jsonDocHttp["externalButtonD12Pressed"] = (digitalRead(EXTERNAL_BUTTON_D12_PIN) == LOW);
    jsonDocHttp["irDpadMode"] = irDpadModes[irDpadMode]; // IR D-Pad Modu eklendi
    jsonDocHttp["sdCardOnline"] = sdCardOnline;
    jsonDocHttp["ledsOn"] = ledsOn;
    jsonDocHttp["brightness"] = globalBrightness;
    jsonDocHttp["ledR"] = currentLedR;
    jsonDocHttp["ledG"] = currentLedG;
    jsonDocHttp["ledB"] = currentLedB;
    jsonDocHttp["dfPlayerMuted"] = dfPlayerMuted; // Mute durumu eklendi

    if(dfPlayerAvailable) {
        jsonDocHttp["dfVolume"] = myDFPlayer.readVolume(); 
        int track = myDFPlayer.readCurrentFileNumber();
        jsonDocHttp["dfTrack"] = track;
        if (track > 0 && track <= totalTracks) { jsonDocHttp["dfTrackName"] = trackNames[track]; } 
        else { jsonDocHttp["dfTrackName"] = trackNames[0]; }
    } else {
        jsonDocHttp["dfVolume"] = -1; jsonDocHttp["dfTrack"] = -1; jsonDocHttp["dfTrackName"] = trackNames[0];
    }
    String response; serializeJson(jsonDocHttp, response); server.send(200, "application/json", response);
}
void handleToggleLed() { /* Same */ ledsOn = !ledsOn; updateLeds(); server.send(200, "text/plain", "OK"); publishStatusMQTT(); }
void handleSetBrightness() { /* Same */ if (server.hasArg("value")) { globalBrightness = server.arg("value").toInt(); updateLeds(); } server.send(200, "text/plain", "OK"); publishStatusMQTT(); }
void handleSetLedRGB() { /* Same */ if (server.hasArg("r") && server.hasArg("g") && server.hasArg("b")) { currentLedR = server.arg("r").toInt(); currentLedG = server.arg("g").toInt(); currentLedB = server.arg("b").toInt(); if (!ledsOn && (currentLedR > 0 || currentLedG > 0 || currentLedB > 0) ) { ledsOn = true; } updateLeds(); } server.send(200, "text/plain", "OK"); publishStatusMQTT(); }
void handleSetDFVolume() { /* Same */ if (dfPlayerAvailable && server.hasArg("value")) { currentDFPlayerVolume = server.arg("value").toInt(); myDFPlayer.volume(currentDFPlayerVolume); dfPlayerMuted = (currentDFPlayerVolume == 0); previousDFPlayerVolume = currentDFPlayerVolume > 0 ? currentDFPlayerVolume : previousDFPlayerVolume; } server.send(200, "text/plain", "OK"); publishStatusMQTT(); }
void handlePlayTrack() { /* Same */ if (dfPlayerAvailable && server.hasArg("track")) { int trackNum = server.arg("track").toInt(); if (trackNum >= 1 && trackNum <= totalTracks) { myDFPlayer.play(trackNum); currentTrackNumber = trackNum; dfPlayerMuted = false; } } server.send(200, "text/plain", "OK"); publishStatusMQTT(); }
void handleDFCommand(const char* cmd) { /* Same */ if (!dfPlayerAvailable) { server.send(503, "text/plain", "DFPlayer Not Available"); return; } bool statusShouldChange = true; if (strcmp(cmd, "play_current") == 0) { if (currentTrackNumber <= 0 || currentTrackNumber > totalTracks) { myDFPlayer.play(1); currentTrackNumber = 1; } else { myDFPlayer.start(); } dfPlayerMuted = false; } else if (strcmp(cmd, "pause") == 0) myDFPlayer.pause(); else if (strcmp(cmd, "next") == 0) { myDFPlayer.next(); currentTrackNumber = 0; dfPlayerMuted = false; } else if (strcmp(cmd, "prev") == 0) { myDFPlayer.previous(); currentTrackNumber = 0; dfPlayerMuted = false; } else { statusShouldChange = false; } server.send(200, "text/plain", "OK"); if(statusShouldChange) publishStatusMQTT(); }

void handleDFMuteToggle() { // Mute için yeni web handler
    if (dfPlayerAvailable) {
        if (dfPlayerMuted) { 
            myDFPlayer.volume(previousDFPlayerVolume > 0 ? previousDFPlayerVolume : 15); 
            currentDFPlayerVolume = previousDFPlayerVolume > 0 ? previousDFPlayerVolume : 15;
            dfPlayerMuted = false;
            Serial.println("WEB: DFPlayer Unmuted. Volume set to: " + String(currentDFPlayerVolume));
        } else { 
            previousDFPlayerVolume = myDFPlayer.readVolume();
            if (previousDFPlayerVolume == -1 || previousDFPlayerVolume == 0) previousDFPlayerVolume = currentDFPlayerVolume > 0 ? currentDFPlayerVolume : 15;
            myDFPlayer.volume(0);
            currentDFPlayerVolume = 0; 
            dfPlayerMuted = true;
            Serial.println("WEB: DFPlayer Muted. Previous volume was: " + String(previousDFPlayerVolume));
        }
    }
    server.send(200, "text/plain", "OK");
    publishStatusMQTT();
}

void handleIrActionFromWeb() { // Yeni web handler IR simülasyonu için
    if (server.hasArg("cmd")) {
        String cmdName = server.arg("cmd");
        uint32_t hexCode = getHexForIrAction(cmdName);
        if (hexCode != 0) {
            Serial.print("WEB: Simulating IR command: "); Serial.println(cmdName);
            processIRCode(hexCode); 
        } else {
            Serial.print("WEB: Unknown IR action: "); Serial.println(cmdName);
        }
    }
    server.send(200, "text/plain", "OK");
    publishStatusMQTT(); 
}

void handleNotFound() { /* Same */ server.send(404, "text/plain", "Not Found"); }