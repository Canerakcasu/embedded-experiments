#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <ESP32Encoder.h>


const char* ssid = "WIFI_NAME";
const char* password = "WIFI_PASSWORD";


const char* mqtt_server = "MQTT SERVER IP";
const int mqtt_port = 1883;
const char* mqtt_topic_set_single_led = "esp32/setSingleLed"; 
const char* mqtt_topic_brightness_feedback = "esp32/brightnessFeedback"; 
const char* mqtt_client_id = "esp32_adv_led_client";


#define ARGB_LED_PIN 33
#define ARGB_LED_COUNT 15
Adafruit_NeoPixel strip(ARGB_LED_COUNT, ARGB_LED_PIN, NEO_GRB + NEO_KHZ800);


#define ENCODER_CLK_PIN 32
#define ENCODER_DT_PIN  35
#define ENCODER_SW_PIN  34


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


void setup_wifi() { 
  delay(10); Serial.println(); Serial.print("Connecting to "); Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println(""); Serial.println("WiFi connected"); Serial.print("IP address: "); Serial.println(WiFi.localIP());
}


uint32_t hexToColor(String hex) { 
  if (hex.startsWith("#")) { hex = hex.substring(1); }
  if (hex.length() != 6) { Serial.print("Invalid HEX: "); Serial.println(hex); return strip.Color(0,0,0); }
  long number = strtol(hex.c_str(), NULL, 16);
  byte r = (number >> 16) & 0xFF; byte g = (number >> 8) & 0xFF; byte b = number & 0xFF;
  return strip.Color(r, g, b);
}


uint32_t adjustBrightness(uint32_t baseColor, int brightness) {
  if (brightness < 0) brightness = 0;
  if (brightness > 100) brightness = 100;

  if (brightness == 0) return strip.Color(0, 0, 0); 

  uint8_t r = (baseColor >> 16) & 0xFF;
  uint8_t g = (baseColor >> 8) & 0xFF;
  uint8_t b = baseColor & 0xFF;

  // Parlaklığı uygula
  r = (r * brightness) / 100;
  g = (g * brightness) / 100;
  b = (b * brightness) / 100;

  return strip.Color(r, g, b);
}

void refreshLedStrip() {
  Serial.print("Refreshing strip. Global Brightness: "); Serial.print(currentBrightness);
  Serial.print(" LEDs On: "); Serial.println(ledsOn);

  for (int i = 0; i < ARGB_LED_COUNT; i++) {
    if (ledsOn) {
      strip.setPixelColor(i, adjustBrightness(baseLedColors[i], currentBrightness));
    } else {
      strip.setPixelColor(i, strip.Color(0, 0, 0)); 
    }
  }
  strip.show();
}


void publishBrightnessFeedback() {
  if (!client.connected()) return;
  StaticJsonDocument<64> doc;
  doc["brightness"] = ledsOn ? currentBrightness : 0; 
  char buffer[64];
  serializeJson(doc, buffer);
  client.publish(mqtt_topic_brightness_feedback, buffer);
  Serial.print("Published brightness feedback: "); Serial.println(buffer);
}

// --- Gelen MQTT Mesajlarını İşleme Fonksiyonu ---
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived ["); Serial.print(topic); Serial.print("] ");
  String messageTemp; for (int i = 0; i < length; i++) { messageTemp += (char)payload[i]; }
  Serial.println(messageTemp);

  if (String(topic) == mqtt_topic_set_single_led) {
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error) { Serial.print(F("deserializeJson() failed: ")); Serial.println(error.f_str()); return; }

    if (!doc.containsKey("color") || !doc.containsKey("index")) {
      Serial.println("Missing 'index' or 'color' in JSON payload.");
      return;
    }

    int ledIndex = doc["index"];
    String hexColor = doc["color"];

    if (ledIndex >= 0 && ledIndex < ARGB_LED_COUNT) {
      baseLedColors[ledIndex] = hexToColor(hexColor); 
      Serial.print("Base color for LED "); Serial.print(ledIndex); Serial.print(" set to: "); Serial.println(hexColor);
      refreshLedStrip(); 
    } else if (ledIndex == -1) { 
        uint32_t newBaseColor = hexToColor(hexColor);
        for(int i=0; i<ARGB_LED_COUNT; i++){
            baseLedColors[i] = newBaseColor;
        }
        Serial.print("Base color for ALL LEDs set to: "); Serial.println(hexColor);
        refreshLedStrip();
    }else {
      Serial.print("Invalid LED index received: "); Serial.println(ledIndex);
    }
  }
}


void reconnect_mqtt() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(mqtt_client_id)) {
      Serial.println("connected"); client.subscribe(mqtt_topic_set_single_led);
      Serial.print("Subscribed to: "); Serial.println(mqtt_topic_set_single_led);
    } else {
      Serial.print("failed, rc="); Serial.print(client.state()); Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  for (int i = 0; i < ARGB_LED_COUNT; i++) {
    baseLedColors[i] = strip.Color(0, 0, 0); 
  }

  encoder.attachHalfQuad(ENCODER_DT_PIN, ENCODER_CLK_PIN);
  encoder.setCount(currentBrightness);
  oldEncoderPosition = currentBrightness;

  pinMode(ENCODER_SW_PIN, INPUT); 

  strip.begin();
  strip.setBrightness(255); 
                            
  refreshLedStrip(); 

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  Serial.println("Setup complete. ESP32 ready for Individual Color & Global Brightness control.");
  publishBrightnessFeedback(); 
}

void loop() {
  if (!client.connected()) {
    reconnect_mqtt();
  }
  client.loop();

  long newEncoderPosition = encoder.getCount();

  if (newEncoderPosition < 0)   { newEncoderPosition = 0;   encoder.setCount(0); }
  if (newEncoderPosition > 100) { newEncoderPosition = 100; encoder.setCount(100); }

  if (newEncoderPosition != oldEncoderPosition) {
    currentBrightness = newEncoderPosition;
    oldEncoderPosition = currentBrightness;

    Serial.print("Global Brightness changed to: "); Serial.println(currentBrightness);

    if (ledsOn) {
      refreshLedStrip(); 
    }
    publishBrightnessFeedback(); 
  }

  bool reading = digitalRead(ENCODER_SW_PIN);
  bool isPressed = (reading == LOW);

  if (isPressed != lastSwitchState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (isPressed != switchState) {
      switchState = isPressed;
      if (switchState == true) { 
        Serial.println("Encoder Switch Pressed!");
        ledsOn = !ledsOn;
        if (ledsOn) {
          currentBrightness = (lastBrightnessBeforeOff > 0) ? lastBrightnessBeforeOff : 1; 
          encoder.setCount(currentBrightness); 
          Serial.print("LEDs turned ON. Brightness set to: "); Serial.println(currentBrightness);
        } else {
          lastBrightnessBeforeOff = currentBrightness; 
          Serial.println("LEDs turned OFF.");
        }
        refreshLedStrip(); 
        publishBrightnessFeedback(); 
      }
    }
  }
  lastSwitchState = isPressed;

  delay(10);
}
