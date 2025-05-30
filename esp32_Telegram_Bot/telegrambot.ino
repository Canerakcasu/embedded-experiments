#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <FastLED.h>
#include <stdio.h>

const char* WIFI_SSID = "Your_WiFi_SSID";
const char* WIFI_PASSWORD = "Your_WiFi_Password";


#define BOT_TOKEN "Your_Telegram_Bot_Token"
#define CHAT_ID "Your_Numerical_Chat_ID"

#define ARGB_LED_PIN 2
#define TOTAL_LEDS 15
#define LEDS_PER_GROUP 3 

CRGB leds[TOTAL_LEDS];
uint8_t currentR = 255;
uint8_t currentG = 255;
uint8_t currentB = 255;
uint8_t currentBrightness = 128;
bool stripActive = false;

const unsigned long BOT_MTBS = 1000; 

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime; 

void setStripColor(uint8_t r, uint8_t g, uint8_t b) {
  currentR = r;
  currentG = g;
  currentB = b;
  if (stripActive) {
    for (int i = 0; i < TOTAL_LEDS; i++) {
      leds[i] = CRGB(r, g, b);
    }
    FastLED.show();
  }
}

void setStripBrightness(uint8_t brightness) {
  currentBrightness = brightness;
  FastLED.setBrightness(currentBrightness);
  if (stripActive) {
    FastLED.show();
  }
}

void turnStripOn() {
  stripActive = true;
  for (int i = 0; i < TOTAL_LEDS; i++) {
    leds[i] = CRGB(currentR, currentG, currentB);
  }
  FastLED.setBrightness(currentBrightness);
  FastLED.show();
}

void turnStripOff() {
  stripActive = false;
  for (int i = 0; i < TOTAL_LEDS; i++) {
    leds[i] = CRGB::Black;
  }
  FastLED.show();
}

void handleNewMessages(int numNewMessages) {
  Serial.print("handleNewMessages ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id_from_message = bot.messages[i].chat_id;
    String text = bot.messages[i].text;
    Serial.println(text);
    String from_name = bot.messages[i].from_name;
    if (from_name == "")
      from_name = "Guest";

    if (chat_id_from_message != CHAT_ID){
      bot.sendMessage(chat_id_from_message, "Unauthorized user", "");
      continue;
    }

    String lowerText = text;
    lowerText.toLowerCase();

    if (lowerText == "/start") {
      String welcome = "Welcome, " + from_name + ".\n";
      welcome += "ARGB LED Strip commands:\n\n";
      welcome += "ledon (or /strip_on) -> Turns the strip ON\n"; 
      welcome += "ledoff (or /strip_off) -> Turns the strip OFF\n"; 
      welcome += "/rgb R G B -> Sets color (e.g., /rgb 255 0 100)\n";
      welcome += "/brightness VAL -> Sets brightness (0-255)\n";
      welcome += "ledstatus (or /strip_status) -> Shows current status\n"; 
      bot.sendMessage(chat_id_from_message, welcome, "Markdown");
    } else if (lowerText == "ledon" || lowerText == "/strip_on") { 
      turnStripOn();
      bot.sendMessage(chat_id_from_message, "LED Strip is ON.", "");
      Serial.println("LED Strip ON");
    } else if (lowerText == "ledoff" || lowerText == "/strip_off") { 
      turnStripOff();
      bot.sendMessage(chat_id_from_message, "LED Strip is OFF.", "");
      Serial.println("LED Strip OFF");
    } else if (lowerText.startsWith("/rgb ")) { 
      int r_val, g_val, b_val;
      int num_parsed = sscanf(text.c_str() + 5, "%d %d %d", &r_val, &g_val, &b_val);
      if (num_parsed == 3 && r_val >= 0 && r_val <= 255 && g_val >= 0 && g_val <= 255 && b_val >= 0 && b_val <= 255) {
        setStripColor(r_val, g_val, b_val);
        if (!stripActive) turnStripOn();
        String msg = "LED color set: R=" + String(r_val) + " G=" + String(g_val) + " B=" + String(b_val);
        bot.sendMessage(chat_id_from_message, msg, "");
        Serial.println(msg);
      } else {
        bot.sendMessage(chat_id_from_message, "Invalid format. Use: /rgb R G B (0-255)", "");
      }
    } else if (lowerText.startsWith("/brightness ")) { 
      int brightness_val;
      int num_parsed = sscanf(text.c_str() + 12, "%d", &brightness_val);
      if (num_parsed == 1 && brightness_val >= 0 && brightness_val <= 255) {
        setStripBrightness(brightness_val);
        String msg = "LED brightness set: " + String(brightness_val);
        bot.sendMessage(chat_id_from_message, msg, "");
        Serial.println(msg);
      } else {
        bot.sendMessage(chat_id_from_message, "Invalid format. Use: /brightness VAL (0-255)", "");
      }
    } else if (lowerText == "ledstatus" || lowerText == "/strip_status") { 
      String status_msg = "LED Strip Status:\n";
      status_msg += stripActive ? "State: ON\n" : "State: OFF\n";
      status_msg += "Color: R=" + String(currentR) + " G=" + String(currentG) + " B=" + String(currentB) + "\n";
      status_msg += "Brightness: " + String(currentBrightness);
      bot.sendMessage(chat_id_from_message, status_msg, "");
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  FastLED.addLeds<NEOPIXEL, ARGB_LED_PIN>(leds, TOTAL_LEDS);
  FastLED.setBrightness(currentBrightness);
  turnStripOff(); 
  delay(10);

  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); 
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  if (millis() - bot_lasttime > BOT_MTBS) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    bot_lasttime = millis();
  }
}