#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <HardwareSerial.h>
#include "DFRobotDFPlayerMini.h"
#include <AiEsp32RotaryEncoder.h>
#include <Preferences.h>

const char* ssid = "Ents_Test";
const char* password = "12345678";
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
    strip.Color(255, 0, 0),   
    strip.Color(0, 255, 0),   
    strip.Color(0, 0, 255),   
    strip.Color(255, 255, 0), 
    strip.Color(0, 255, 255), 
    strip.Color(255, 0, 255), 
    strip.Color(255, 255, 255) 
};
const int num_predefined_colors = sizeof(predefinedColors) / sizeof(predefinedColors[0]);
int currentPredefinedColorIndex = 0;

HardwareSerial myDFPlayerSerial(2);
DFRobotDFPlayerMini myDFPlayer;
bool dfPlayerAvailable = false;
bool sdCardOnline = false;
int currentDFPlayerVolume = 20;
int currentTrackNumber = 0;
const int totalTracks = 5; 

// !!! ÖNEMLİ: LÜTFEN BU İSİMLERİ KENDİ MP3 DOSYA İSİMLERİNİZLE DEĞİŞTİRİN !!!
const char* trackNames[totalTracks + 1] = {
    "Unknown/Stopped",      
    "Track 1 (e.g. Intro)", 
    "Track 2 (e.g. Birds)", 
    "Track 3 (e.g. Melody)",
    "Track 4 (e.g. Effect)",
    "Track 5 (e.g. Ambiance)"
};

#define DFPLAYER_RX_PIN 16
#define DFPLAYER_TX_PIN 17

#define EXTERNAL_BUTTON_D12_PIN 12 // GPIO 12 için yeni buton tanımı

StaticJsonDocument<1024> jsonDoc;

const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><title>ESP32 Advanced Control</title><meta name="viewport" content="width=device-width, initial-scale=1"><style>body{font-family:'Roboto',Arial,sans-serif;background-color:#121212;color:#e0e0e0;padding:15px;display:flex;flex-direction:column;align-items:center;margin:0}h1{color:#e53935;text-align:center;text-shadow:1px 1px 3px #000;margin-bottom:25px}h3{color:#ff7961;text-align:left;border-bottom:2px solid #b71c1c;padding-bottom:8px;margin-top:20px;margin-bottom:15px}.grid-container{display:grid;grid-template-columns:repeat(auto-fit, minmax(340px, 1fr));gap:25px;width:100%;max-width:1100px}.card{background-color:#1e1e1e;border:1px solid #c21807;border-radius:10px;padding:20px;box-shadow:0 6px 12px rgba(0,0,0,0.5)}button{background-color:#b71c1c;color:white;border:none;padding:12px 18px;margin:6px;border-radius:6px;cursor:pointer;font-size:16px;font-weight:700;transition:background-color .2s,transform .1s;box-shadow:0 2px 5px rgba(0,0,0,0.4)}button:hover{background-color:#f44336;transform:translateY(-2px)}button:active{transform:translateY(0);box-shadow:inset 0 1px 3px rgba(0,0,0,0.5)}.status-item{margin-bottom:10px;font-size:1.1em;padding:5px 0}.status-label{font-weight:700;color:#ff8a80}.track-buttons button{width:48px;height:48px;margin:5px;background-color:#424242;border:1px solid #e53935}.track-buttons button:hover{background-color:#e53935;border:1px solid #ff6b6b}.color-button{width:38px;height:38px;margin:5px;border:2px solid #121212;border-radius:50%;padding:0;cursor:pointer;transition:transform .2s,border-color .2s}.color-button:hover{border-color:#ff6b6b;transform:scale(1.1)}.color-preview{width:60px;height:28px;border:2px solid #ff6b6b;display:inline-block;vertical-align:middle;margin-left:10px;border-radius:5px}input[type=color]{width:50px;height:35px;border:1px solid #555;border-radius:4px;padding:2px;cursor:pointer;vertical-align:middle;margin-left:10px}input[type=range]{width:calc(100% - 180px);margin:0 10px;vertical-align:middle;cursor:pointer;-webkit-appearance:none;appearance:none;height:10px;background:#424242;border-radius:5px;outline:none;transition:background .3s}input[type=range]:hover{background:#535353}input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;appearance:none;width:22px;height:22px;background:#e53935;border-radius:50%;cursor:pointer;border:2px solid #121212}input[type=range]::-moz-range-thumb{width:20px;height:20px;background:#e53935;border-radius:50%;cursor:pointer;border:none}label{vertical-align:middle;margin-right:5px}.controls-row{display:flex;flex-wrap:wrap;align-items:center;margin-bottom:12px;gap:10px}</style></head><body><h1>ESP32 Advanced Control Panel</h1><div class="grid-container"><div class="card"><h3>General Status</h3><div class="status-item"><span class="status-label">WiFi Network: </span><span id="wifiSSID">Loading...</span></div><div class="status-item"><span class="status-label">Active Encoder Mode: </span><span id="encoderModeStatus">Loading...</span></div><div class="status-item"><span class="status-label">Encoder Button (GPIO14): </span><span id="encoderButtonStatus">Not Pressed</span></div><div class="status-item"><span class="status-label">External Button (GPIO12): </span><span id="externalButtonD12Status">Not Pressed</span></div><div class="status-item"><span class="status-label">SD Card: </span><span id="sdStatus">Loading...</span></div></div><div class="card"><h3>LED Control</h3><div class="controls-row"><button id="ledToggleBtn" onclick="sendCommand('/toggle_led')">Toggle LEDs</button></div><div class="controls-row"><label for="brightnessSlider" class="status-label">Brightness:</label><input type="range" id="brightnessSlider" min="0" max="255" value="120" oninput="updateBrightnessValue(this.value)" onchange="setBrightness(this.value)"><span id="brightnessValue">120</span></div><div class="status-item controls-row"><span class="status-label">Current Color:</span><input type="color" id="rgbColorPicker" onchange="setRGBColor(this.value)"><div id="currentColorPreview" class="color-preview"></div></div></div><div class="card"><h3>DFPlayer Control</h3><div class="status-item"><span class="status-label">Volume Level: </span><span id="volumeStatus">Loading...</span></div><div class="status-item"><span class="status-label">Playing Track: </span><span id="trackStatus">Loading...</span> (<span id="trackNameStatus"></span>)</div><div class="controls-row"><label for="dfVolumeSlider" class="status-label">Set Volume:</label><input type="range" id="dfVolumeSlider" min="0" max="30" value="20" oninput="updateDfVolumeValue(this.value)" onchange="setDFVolume(this.value)"><span id="dfVolumeValue">20</span></div><div class="controls-row button-group"><button onclick="sendCommand('/df_play_current')">Play/Resume</button><button onclick="sendCommand('/df_pause')">Pause</button><button onclick="sendCommand('/df_next')">Next</button><button onclick="sendCommand('/df_prev')">Previous</button></div><div><p class="status-label">Select Track:</p><span id="trackButtonsContainer"></span></div></div></div><script>const totalTracks=5;function sendCommand(url){fetch(url).then(response=>{if(!response.ok)console.error('Command Error:',response.status);return response.text()}).then(data=>console.log(url+'->'+data)).catch(error=>console.error('Fetch Error:',error));setTimeout(updateStatus,250)}function setBrightness(value){sendCommand('/set_brightness?value='+value)}function updateBrightnessValue(value){document.getElementById('brightnessValue').textContent=value}function setRGBColor(hexColor){const r=parseInt(hexColor.substr(1,2),16);const g=parseInt(hexColor.substr(3,2),16);const b=parseInt(hexColor.substr(5,2),16);sendCommand(`/set_led_rgb?r=${r}&g=${g}&b=${b}`)}function setDFVolume(volume){sendCommand('/set_df_volume?value='+volume)}function updateDfVolumeValue(value){document.getElementById('dfVolumeValue').textContent=value}function playTrack(trackNum){sendCommand('/play_track?track='+trackNum)}function updateStatus(){fetch('/status').then(response=>response.json()).then(data=>{document.getElementById('wifiSSID').textContent=data.wifiSSID||"Not Connected";document.getElementById('encoderModeStatus').textContent=data.encoderMode;document.getElementById('encoderButtonStatus').textContent=data.encoderButtonPressed?"Pressed":"Not Pressed";document.getElementById('externalButtonD12Status').textContent=data.externalButtonD12Pressed?"Pressed":"Not Pressed";document.getElementById('sdStatus').textContent=data.sdCardOnline?"Online":"Offline/Error";document.getElementById('ledToggleBtn').textContent=data.ledsOn?"Turn LEDs Off":"Turn LEDs On";const brightnessSlider=document.getElementById('brightnessSlider');if(document.activeElement!==brightnessSlider){brightnessSlider.value=data.brightness}document.getElementById('brightnessValue').textContent=data.brightness;const colorHex='#'+[data.ledR,data.ledG,data.ledB].map(c=>parseInt(c).toString(16).padStart(2,'0')).join('');document.getElementById('currentColorPreview').style.backgroundColor=colorHex;const colorPicker=document.getElementById('rgbColorPicker');if(document.activeElement!==colorPicker)colorPicker.value=colorHex;document.getElementById('volumeStatus').textContent=data.dfVolume!==-1?data.dfVolume:"N/A";const dfVolumeSlider=document.getElementById('dfVolumeSlider');if(document.activeElement!==dfVolumeSlider){dfVolumeSlider.value=data.dfVolume!==-1?data.dfVolume:20}document.getElementById('dfVolumeValue').textContent=data.dfVolume!==-1?data.dfVolume:20;document.getElementById('trackStatus').textContent=data.dfTrack!==-1&&data.dfTrack!==0?data.dfTrack:"Stopped";document.getElementById('trackNameStatus').textContent=data.dfTrackName||"";console.log("Status updated:",data)}).catch(error=>console.error('Could not get status:',error))}function createTrackButtons(){const container=document.getElementById('trackButtonsContainer');if(container){container.innerHTML='';for(let i=1;i<=totalTracks;i++){const btn=document.createElement('button');btn.textContent=i;btn.onclick=function(){playTrack(i)};container.appendChild(btn)}}}window.onload=()=>{createTrackButtons();updateStatus();setInterval(updateStatus,2200)};</script></body></html>
)rawliteral";

void setupHardware();
void setupWiFi();
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
void handleNotFound();

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n--- ESP32 Advanced Control Panel ---");
    setupHardware();
    setupWiFi();
    setupWebServer();
    Serial.println("Setup Complete. IP Address:");
    Serial.println(WiFi.localIP());
    Serial.println("------------------------------------");
}

void loop() {
    server.handleClient();
    handleRotaryEncoder();
    if (dfPlayerAvailable && myDFPlayer.available()) {
        uint8_t type = myDFPlayer.readType();
        int value = myDFPlayer.read();
        switch (type) {
            case DFPlayerPlayFinished:
                Serial.print("Track Finished: "); Serial.println(value);
                currentTrackNumber = 0; 
                break;
            case DFPlayerError:
                Serial.print("DFPlayer Error, Code: "); Serial.println(value);
                break;
            case DFPlayerCardOnline:
                sdCardOnline = true;
                break;
            case DFPlayerCardRemoved:
                sdCardOnline = false;
                break;
        }
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

    pinMode(EXTERNAL_BUTTON_D12_PIN, INPUT_PULLUP); // Yeni butonun pin modu
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
    }
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
        Serial.println("\nWiFi Connection Failed. Web Server will not start.");
    }
}

void setupWebServer() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Web Server not started due to WiFi connection failure.");
        return;
    }
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
    server.onNotFound(handleNotFound);
    server.begin();
    Serial.println("HTTP Server Started.");
}

void updateLeds() {
    if (ledsOn) {
        strip.setBrightness(globalBrightness);
        strip.fill(strip.Color(currentLedR, currentLedG, currentLedB));
    } else {
        strip.fill(strip.Color(0,0,0));
    }
    strip.show();
}

void handleRotaryEncoder() {
    if (rotaryEncoder.encoderChanged()) {
        int value = rotaryEncoder.readEncoder();
        switch (currentEncoderMode) {
            case 0: 
                globalBrightness = value;
                updateLeds();
                break;
            case 1: { 
                currentPredefinedColorIndex = value;
                uint32_t newColor = predefinedColors[currentPredefinedColorIndex];
                currentLedR = (newColor >> 16) & 0xFF;
                currentLedG = (newColor >> 8) & 0xFF;
                currentLedB = newColor & 0xFF;
                updateLeds();
                break;
            }
            case 2: 
                currentDFPlayerVolume = value;
                if (dfPlayerAvailable) myDFPlayer.volume(currentDFPlayerVolume);
                break;
        }
    }
    if (rotaryEncoder.isEncoderButtonClicked()) {
        currentEncoderMode = (currentEncoderMode + 1) % 3;
        Serial.print("Encoder Mode Changed to: "); Serial.println(encoderModes[currentEncoderMode]);
        switch (currentEncoderMode) {
            case 0:
                rotaryEncoder.setBoundaries(0, 255, false);
                rotaryEncoder.setEncoderValue(globalBrightness);
                break;
            case 1:
                rotaryEncoder.setBoundaries(0, num_predefined_colors - 1, true);
                rotaryEncoder.setEncoderValue(currentPredefinedColorIndex);
                break;
            case 2:
                rotaryEncoder.setBoundaries(0, 30, false);
                if(dfPlayerAvailable) {
                    int volRead = myDFPlayer.readVolume();
                    currentDFPlayerVolume = (volRead == -1 ? currentDFPlayerVolume : volRead); 
                } else {
                     currentDFPlayerVolume = 20;
                }
                rotaryEncoder.setEncoderValue(currentDFPlayerVolume);
                break;
        }
        delay(100);
    }
}

void handleRoot() {
    server.send_P(200, "text/html", INDEX_HTML);
}

void handleStatus() {
    jsonDoc.clear();
    jsonDoc["wifiSSID"] = (WiFi.status() == WL_CONNECTED) ? WiFi.SSID() : "Not Connected";
    jsonDoc["encoderMode"] = encoderModes[currentEncoderMode];
    jsonDoc["encoderButtonPressed"] = (digitalRead(ROTARY_ENCODER_BUTTON_PIN) == LOW);
    jsonDoc["externalButtonD12Pressed"] = (digitalRead(EXTERNAL_BUTTON_D12_PIN) == LOW); // Yeni buton durumu
    jsonDoc["sdCardOnline"] = sdCardOnline;
    jsonDoc["ledsOn"] = ledsOn;
    jsonDoc["brightness"] = globalBrightness;
    jsonDoc["ledR"] = currentLedR;
    jsonDoc["ledG"] = currentLedG;
    jsonDoc["ledB"] = currentLedB;

    if(dfPlayerAvailable) {
        jsonDoc["dfVolume"] = myDFPlayer.readVolume();
        int track = myDFPlayer.readCurrentFileNumber();
        jsonDoc["dfTrack"] = track;
        if (track > 0 && track <= totalTracks) {
            jsonDoc["dfTrackName"] = trackNames[track];
        } else {
            jsonDoc["dfTrackName"] = trackNames[0];
        }
    } else {
        jsonDoc["dfVolume"] = -1;
        jsonDoc["dfTrack"] = -1;
        jsonDoc["dfTrackName"] = trackNames[0];
    }
    String response;
    serializeJson(jsonDoc, response);
    server.send(200, "application/json", response);
}

void handleToggleLed() {
    ledsOn = !ledsOn;
    updateLeds();
    server.send(200, "text/plain", "OK");
}

void handleSetBrightness() {
    if (server.hasArg("value")) {
        globalBrightness = server.arg("value").toInt();
        updateLeds();
    }
    server.send(200, "text/plain", "OK");
}

void handleSetLedRGB() {
    if (server.hasArg("r") && server.hasArg("g") && server.hasArg("b")) {
        currentLedR = server.arg("r").toInt();
        currentLedG = server.arg("g").toInt();
        currentLedB = server.arg("b").toInt();
        if (!ledsOn && (currentLedR > 0 || currentLedG > 0 || currentLedB > 0) ) {
            ledsOn = true;
        }
        updateLeds();
    }
    server.send(200, "text/plain", "OK");
}

void handleSetDFVolume() {
    if (dfPlayerAvailable && server.hasArg("value")) {
        currentDFPlayerVolume = server.arg("value").toInt();
        myDFPlayer.volume(currentDFPlayerVolume);
    }
    server.send(200, "text/plain", "OK");
}

void handlePlayTrack() {
    if (dfPlayerAvailable && server.hasArg("track")) {
        int trackNum = server.arg("track").toInt();
        if (trackNum >= 1 && trackNum <= totalTracks) {
            myDFPlayer.play(trackNum);
            currentTrackNumber = trackNum; 
        }
    }
    server.send(200, "text/plain", "OK");
}

void handleDFCommand(const char* cmd) {
    if (!dfPlayerAvailable) { server.send(503, "text/plain", "DFPlayer Not Available"); return; }
    if (strcmp(cmd, "play_current") == 0) {
        if (currentTrackNumber <= 0 || currentTrackNumber > totalTracks) { 
            myDFPlayer.play(1); 
            currentTrackNumber = 1;
        } else {
             myDFPlayer.start(); 
        }
    }
    else if (strcmp(cmd, "pause") == 0) myDFPlayer.pause();
    else if (strcmp(cmd, "next") == 0) {
        myDFPlayer.next();
        currentTrackNumber = 0; 
    }
    else if (strcmp(cmd, "prev") == 0) {
        myDFPlayer.previous();
        currentTrackNumber = 0; 
    }
    server.send(200, "text/plain", "OK");
}

void handleNotFound() {
    server.send(404, "text/plain", "Not Found");
}