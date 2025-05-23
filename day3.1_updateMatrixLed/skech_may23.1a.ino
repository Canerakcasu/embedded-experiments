#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <ESP32Encoder.h>
#include <MD_Parola.h>      // --- ADDED FOR MATRIX DISPLAY ---
#include <MD_MAX72xx.h>     // --- ADDED FOR MATRIX DISPLAY ---
#include <SPI.h>            // --- ADDED FOR MATRIX DISPLAY (SPI) ---

// --- Wi-Fi Settings ---
const char* ssid = "WIFI_NAME";         
const char* password = "WIFI_PASSWORD";    

// --- MQTT Broker Settings ---
const char* mqtt_server = "xxxxxxxxxxxxxxxx"; // <<< ENTER YOUR MQTT BROKER IP HERE
const int mqtt_port = 1883;
const char* mqtt_topic_set_single_led = "esp32/setSingleLed";
const char* mqtt_topic_brightness_feedback = "esp32/brightnessFeedback";
// const char* mqtt_topic_matrix_message = "esp32/matrix/message"; // This topic is no longer used for this display logic
const char* mqtt_client_id = "esp32_status_matrix_client_en";

// --- ARGB LED Strip Settings ---
#define ARGB_LED_PIN 33
#define ARGB_LED_COUNT 15
Adafruit_NeoPixel strip(ARGB_LED_COUNT, ARGB_LED_PIN, NEO_GRB + NEO_KHZ800);

// --- Rotary Encoder Pin Definitions ---
#define ENCODER_CLK_PIN 32
#define ENCODER_DT_PIN  35
#define ENCODER_SW_PIN  34

// --- MAX7219 Pin Definitions --- // --- ADDED FOR MATRIX DISPLAY ---
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW // Change if your module type is different (FC16 is common)
#define MAX_DEVICES 4   // Number of 8x8 modules you have
#define CLK_PIN     2   // Your D2 pin for CLK
#define DATA_PIN    12  // Your D12 pin for DIN
#define CS_PIN      13  // Your D13 pin for CS

// --- MD_Parola & MD_MAX72XX Setup --- // --- ADDED FOR MATRIX DISPLAY ---
MD_Parola P = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

// --- Global Variables for Matrix Display State --- // --- ADDED FOR MATRIX DISPLAY ---
enum DisplayState {
  SHOW_BRIGHTNESS,
  SHOW_LED0_COLOR,
  SHOW_LED1_COLOR,
  SHOW_LED2_COLOR,
  SHOW_LED3_COLOR,
  SHOW_LED4_COLOR
};
DisplayState currentDisplayState = SHOW_BRIGHTNESS;
unsigned long lastDisplayUpdateTime = 0;
const unsigned long DISPLAY_UPDATE_INTERVAL = 3000; // Display each info for 3 seconds
bool forceMatrixUpdate = true; // Flag to force matrix update immediately

// --- Other Global Variables ---
WiFiClient espClient;
PubSubClient client(espClient);
uint32_t baseLedColors[ARGB_LED_COUNT];
ESP32Encoder encoder;
long oldEncoderPosition = -999;
int currentBrightness = 100;
int lastBrightnessBeforeOff = 100;
bool ledsOn = true;
bool switchState = true;
bool lastSwitchState = true;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

// --- Function Prototypes ---
void setup_wifi();
uint32_t hexToColor(String hex);
String colorToHex(uint32_t colorVal); // For Matrix Display
uint32_t adjustBrightness(uint32_t baseColor, int brightness);
void refreshLedStrip();
void publishBrightnessFeedback();
void callback(char* topic, byte* payload, unsigned int length);
void reconnect_mqtt();

// --- Wi-Fi Connection Function ---
void setup_wifi() {
  delay(10); Serial.println(); Serial.print("Connecting to "); Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println(""); Serial.println("WiFi connected"); Serial.print("IP address: "); Serial.println(WiFi.localIP());
}

// --- Hex String to uint32_t Color Conversion ---
uint32_t hexToColor(String hex) {
  if (hex.startsWith("#")) { hex = hex.substring(1); }
  if (hex.length() != 6) { Serial.print("Invalid HEX: "); Serial.println(hex); return strip.Color(0,0,0); }
  long number = strtol(hex.c_str(), NULL, 16);
  byte r = (number >> 16) & 0xFF; byte g = (number >> 8) & 0xFF; byte b = number & 0xFF;
  return strip.Color(r, g, b);
}

// --- uint32_t Color to HEX String Conversion --- // --- ADDED FOR MATRIX DISPLAY ---
String colorToHex(uint32_t colorVal) {
  uint8_t r = (colorVal >> 16) & 0xFF;
  uint8_t g = (colorVal >> 8) & 0xFF;
  uint8_t b = colorVal & 0xFF;
  char hex[8];
  sprintf(hex, "#%02X%02X%02X", r, g, b);
  return String(hex);
}

// --- Adjust Brightness of a Color ---
uint32_t adjustBrightness(uint32_t baseColor, int brightness) {
  if (brightness < 0) brightness = 0; if (brightness > 100) brightness = 100;
  if (brightness == 0) return strip.Color(0, 0, 0);
  uint8_t r = (baseColor >> 16) & 0xFF; uint8_t g = (baseColor >> 8) & 0xFF; uint8_t b = baseColor & 0xFF;
  r = (r * brightness) / 100; g = (g * brightness) / 100; b = (b * brightness) / 100;
  return strip.Color(r, g, b);
}

// --- Refresh ARGB LED Strip ---
void refreshLedStrip() {
  Serial.print("Refreshing strip. Brightness: "); Serial.print(currentBrightness);
  Serial.print(" On: "); Serial.println(ledsOn);
  for (int i = 0; i < ARGB_LED_COUNT; i++) {
    if (ledsOn) {
      strip.setPixelColor(i, adjustBrightness(baseLedColors[i], currentBrightness));
    } else {
      strip.setPixelColor(i, strip.Color(0, 0, 0));
    }
  }
  strip.show();
}

// --- Publish Brightness Feedback via MQTT ---
void publishBrightnessFeedback() {
  if (!client.connected()) return;
  StaticJsonDocument<64> doc;
  doc["brightness"] = ledsOn ? currentBrightness : 0;
  char buffer[64];
  serializeJson(doc, buffer);
  client.publish(mqtt_topic_brightness_feedback, buffer);
  Serial.print("Published brightness feedback: "); Serial.println(buffer);
}

// --- MQTT Message Callback Function ---
void callback(char* topic, byte* payload, unsigned int length) {
  String messageTemp;
  payload[length] = '\0'; // Ensure null termination
  messageTemp = String((char*)payload);
  Serial.print("Message arrived ["); Serial.print(topic); Serial.print("] "); Serial.println(messageTemp);

  if (String(topic) == mqtt_topic_set_single_led) {
      StaticJsonDocument<128> doc;
      DeserializationError error = deserializeJson(doc, payload, length);
      if (error) { Serial.print(F("JSON Deserialize failed (setSingleLed): ")); Serial.println(error.f_str()); return; }
      if (!doc.containsKey("color") || !doc.containsKey("index")) { Serial.println("Missing 'index' or 'color' in setSingleLed payload."); return; }
      
      int ledIndex = doc["index"];
      String hexColor = doc["color"];
      bool colorChangedForMatrix = false;

      if (ledIndex == -1) { // Set all LEDs
          uint32_t newBaseColor = hexToColor(hexColor);
          for(int i=0; i<ARGB_LED_COUNT; i++){ baseLedColors[i] = newBaseColor; }
          colorChangedForMatrix = true; // All colors changed, matrix needs update
          Serial.print("Base color for ALL LEDs set to: "); Serial.println(hexColor);
      }
      else if (ledIndex >= 0 && ledIndex < ARGB_LED_COUNT) { // Set a single LED
          baseLedColors[ledIndex] = hexToColor(hexColor);
          Serial.print("Base color for LED "); Serial.print(ledIndex); Serial.print(" set to: "); Serial.println(hexColor);
          if (ledIndex < 5) colorChangedForMatrix = true; // If one of the first 5 changed, matrix needs update
      }
      else { Serial.print("Invalid LED index received: "); Serial.println(ledIndex); return; }
      
      refreshLedStrip();
      if (colorChangedForMatrix) {
          forceMatrixUpdate = true; // Force matrix update
      }
  }
  // Note: MQTT topic for matrix message (esp32/matrix/message) is removed as per the new display logic
}

// --- MQTT Reconnect Function ---
void reconnect_mqtt() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(mqtt_client_id)) {
      Serial.println("connected");
      client.subscribe(mqtt_topic_set_single_led); // Subscribe to ARGB LED control topic
      Serial.println("Subscribed to setSingleLed topic.");
    } else {
      Serial.print("failed, rc="); Serial.print(client.state()); Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

// --- Setup Function ---
void setup() {
  Serial.begin(115200);
  delay(100);

  // --- Initialize Parola Matrix Display --- // --- ADDED FOR MATRIX DISPLAY ---
  P.begin();
  P.setIntensity(5); // Set matrix brightness (0-15)
  P.displayClear();
  P.setTextAlignment(PA_CENTER); // Center align text

  // Initialize base colors for ARGB strip (e.g., dim red)
  for (int i = 0; i < ARGB_LED_COUNT; i++) { baseLedColors[i] = strip.Color(20,0,0); }

  // Initialize Encoder
  encoder.attachHalfQuad(ENCODER_DT_PIN, ENCODER_CLK_PIN);
  encoder.setCount(currentBrightness);
  oldEncoderPosition = currentBrightness;
  pinMode(ENCODER_SW_PIN, INPUT); // External 10K PULL-UP RESISTOR IS RECOMMENDED!

  // Initialize ARGB Strip
  strip.begin();
  strip.setBrightness(255); // Max out NeoPixel library brightness, control via adjustBrightness()
  refreshLedStrip();      // Apply initial state

  // Connect to Wi-Fi and MQTT
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  Serial.println("Setup complete.");
  publishBrightnessFeedback(); // Publish initial brightness
  lastDisplayUpdateTime = millis(); // Start matrix display timer
  forceMatrixUpdate = true;         // Force initial matrix display
}

// --- Loop Function ---
void loop() {
  if (!client.connected()) {
    reconnect_mqtt();
  }
  client.loop();

  // --- Handle Parola Matrix Animations --- // --- MODIFIED FOR MATRIX DISPLAY ---
  P.displayAnimate();

  // --- Read Rotary Encoder for ARGB Brightness ---
  long newEncoderPosition = encoder.getCount();
  if (newEncoderPosition < 0)   { newEncoderPosition = 0;   encoder.setCount(0); }
  if (newEncoderPosition > 100) { newEncoderPosition = 100; encoder.setCount(100); }
  if (newEncoderPosition != oldEncoderPosition) {
    currentBrightness = newEncoderPosition;
    oldEncoderPosition = currentBrightness;
    Serial.print("Global ARGB Brightness changed to: "); Serial.println(currentBrightness);
    if (ledsOn) {
      refreshLedStrip();
    }
    publishBrightnessFeedback();
    forceMatrixUpdate = true; // Brightness changed, update matrix
  }

  // --- Read Encoder Button for ARGB ON/OFF ---
  bool reading = digitalRead(ENCODER_SW_PIN);
  bool isPressed = (reading == LOW); // Assumes PULL-UP resistor (external or if pin supports internal)
  if (isPressed != lastSwitchState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (isPressed != switchState) {
      switchState = isPressed;
      if (switchState == true) { // Button was just pressed
        Serial.println("Encoder Switch Pressed!");
        ledsOn = !ledsOn;
        if (ledsOn) {
          currentBrightness = (lastBrightnessBeforeOff > 0) ? lastBrightnessBeforeOff : 1;
          encoder.setCount(currentBrightness); // Sync encoder
          Serial.print("ARGB LEDs turned ON. Brightness: "); Serial.println(currentBrightness);
        } else {
          lastBrightnessBeforeOff = currentBrightness; // Save current brightness
          Serial.println("ARGB LEDs turned OFF.");
        }
        refreshLedStrip();
        publishBrightnessFeedback();
        forceMatrixUpdate = true; // LED state changed, update matrix
      }
    }
  }
  lastSwitchState = isPressed;

  // --- Update Matrix Display Cyclically or on Force Update --- // --- MODIFIED FOR MATRIX DISPLAY ---
  if (forceMatrixUpdate || (millis() - lastDisplayUpdateTime > DISPLAY_UPDATE_INTERVAL)) {
    char displayText[40] = ""; // Buffer for text to display on matrix

    switch (currentDisplayState) {
      case SHOW_BRIGHTNESS:
        sprintf(displayText, "B: %d%%", ledsOn ? currentBrightness : 0);
        currentDisplayState = SHOW_LED0_COLOR; // Cycle to next state
        break;
      case SHOW_LED0_COLOR:
        if (0 < ARGB_LED_COUNT) sprintf(displayText, "L0:%s", colorToHex(baseLedColors[0]).c_str());
        else strcpy(displayText, "L0: N/A");
        currentDisplayState = SHOW_LED1_COLOR;
        break;
      case SHOW_LED1_COLOR:
        if (1 < ARGB_LED_COUNT) sprintf(displayText, "L1:%s", colorToHex(baseLedColors[1]).c_str());
        else strcpy(displayText, "L1: N/A");
        currentDisplayState = SHOW_LED2_COLOR;
        break;
      case SHOW_LED2_COLOR:
        if (2 < ARGB_LED_COUNT) sprintf(displayText, "L2:%s", colorToHex(baseLedColors[2]).c_str());
        else strcpy(displayText, "L2: N/A");
        currentDisplayState = SHOW_LED3_COLOR;
        break;
      case SHOW_LED3_COLOR:
        if (3 < ARGB_LED_COUNT) sprintf(displayText, "L3:%s", colorToHex(baseLedColors[3]).c_str());
        else strcpy(displayText, "L3: N/A");
        currentDisplayState = SHOW_LED4_COLOR;
        break;
      case SHOW_LED4_COLOR:
        if (4 < ARGB_LED_COUNT) sprintf(displayText, "L4:%s", colorToHex(baseLedColors[4]).c_str());
        else strcpy(displayText, "L4: N/A");
        currentDisplayState = SHOW_BRIGHTNESS; // Cycle back to brightness
        break;
    }
    
    P.displayText(displayText, PA_CENTER, 50, DISPLAY_UPDATE_INTERVAL - 200, PA_PRINT, PA_NO_EFFECT); // Display text statically
                                                                                              // Effect PA_PRINT, Speed 50ms, Pause until next interval
    Serial.print("Matrix Display Update: "); Serial.println(displayText);
    
    lastDisplayUpdateTime = millis();
    forceMatrixUpdate = false; // Reset the force flag
  }
  delay(20); // General delay for stability
}