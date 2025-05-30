// --- Core Libraries ---
#include <WiFi.h> // Use <ESP8266WiFi.h> for ESP8266
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <AiEsp32RotaryEncoder.h>

// --- WiFi and Telegram Settings (Update with your credentials) ---
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
#define BOT_TOKEN "YOUR_BOT_TOKEN"
#define CHAT_ID "YOUR_CHAT_ID" // Your numerical Telegram Chat ID
const unsigned long BOT_MTBS = 1000; // Mean Time Between Scan messages

// --- Stepper Motor Pin Definitions ---
#define STEP_PIN 23
#define DIR_PIN 22
#define EN_PIN 21 // Enable pin for the stepper driver

// --- Stepper Motor Parameters (Adjust to your motor/driver) ---
#define STEPS_PER_REVOLUTION 200 
#define MICROSTEPPING_FACTOR 1    // e.g., 1 for full step, 2 for half, 8 for 1/8, etc.

// --- Rotary Encoder Pin Definitions ---
#define ROTARY_ENCODER_A_PIN 14
#define ROTARY_ENCODER_B_PIN 33
#define ROTARY_ENCODER_BUTTON_PIN 27
#define ROTARY_ENCODER_STEPS 4 // Pulses per detent for your encoder

// --- Global Variables - Motor & Encoder ---
int stepMotorSpeedDelay = 1000; // Microseconds delay between step pulses (controls speed)
long currentEncoderPosition = 0;
long lastEncoderPosition = 0;
volatile boolean motorGloballyEnabled = false; // Motor active state (controlled by EN_PIN)

// --- Library Objects ---
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime;
AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, -1, ROTARY_ENCODER_STEPS);

// --- Interrupt Service Routine for Rotary Encoder ---
void IRAM_ATTR readEncoderISR() {
  rotaryEncoder.readEncoder_ISR();
}

// --- Stepper Motor Control Functions ---
void enableMotor() {
    digitalWrite(EN_PIN, LOW); // LOW to enable for many common drivers (A4988, DRV8825)
    motorGloballyEnabled = true;
    Serial.println("Motor ENABLED");
}

void disableMotor() {
    digitalWrite(EN_PIN, HIGH); // HIGH to disable for many common drivers
    motorGloballyEnabled = false;
    Serial.println("Motor DISABLED");
}

void stepOnce(bool direction, int speedDelay) {
  if (!motorGloballyEnabled) return; // Only step if motor is enabled
  digitalWrite(DIR_PIN, direction);
  digitalWrite(STEP_PIN, HIGH);
  delayMicroseconds(speedDelay);
  digitalWrite(STEP_PIN, LOW);
  delayMicroseconds(speedDelay);
}

void rotateStepperBySteps(long stepsToMove, bool direction) {
  if (!motorGloballyEnabled) {
    Serial.println("Motor is disabled. Cannot move.");
    // Optionally send a Telegram message back if called from there
    // bot.sendMessage(CHAT_ID, "Motor is disabled. Use 'motoron' first.", "");
    return;
  }
  Serial.print("Rotating by steps: "); Serial.print(stepsToMove);
  Serial.print(" Direction: "); Serial.println(direction ? "CW (HIGH)" : "CCW (LOW)");
  for (long i = 0; i < stepsToMove; i++) {
    stepOnce(direction, stepMotorSpeedDelay);
  }
}

void rotateStepperByDegrees(float degrees) {
  bool direction = HIGH; // Default Clockwise
  if (degrees < 0) {
    direction = LOW; // Counter-Clockwise
    degrees = -degrees;
  }
  long stepsToMove = (long)((degrees / 360.0) * STEPS_PER_REVOLUTION * MICROSTEPPING_FACTOR);
  rotateStepperBySteps(stepsToMove, direction);
}

// --- Telegram Message Handler ---
void handleNewMessages(int numNewMessages) {
  Serial.print("handleNewMessages "); Serial.println(numNewMessages);
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id_from_message = bot.messages[i].chat_id;
    String text = bot.messages[i].text;
    Serial.println(text);
    String from_name = bot.messages[i].from_name;
    if (from_name == "") from_name = "Guest";

    if (chat_id_from_message != CHAT_ID) {
      bot.sendMessage(chat_id_from_message, "Unauthorized user", "");
      continue;
    }

    String lowerText = text;
    lowerText.toLowerCase();

    if (lowerText == "/start") {
      String welcome = "Welcome, " + from_name + ".\n";
      welcome += "Stepper Motor Control Bot:\n\n";
      welcome += "motoron -> Enables the motor driver\n";
      welcome += "motoroff -> Disables the motor driver\n";
      welcome += "turn <degrees> -> Rotates motor (e.g., turn90, turn-45 for CCW)\n";
      welcome += "step <count> -> Rotates motor by steps (e.g., step200, step-100 for CCW)\n";
      welcome += "motorstatus -> Shows if motor driver is enabled\n";
      bot.sendMessage(chat_id_from_message, welcome, "Markdown");
    } else if (lowerText == "motoron") { 
      enableMotor();
      bot.sendMessage(chat_id_from_message, "Motor driver ENABLED.", "");
    } else if (lowerText == "motoroff") { 
      disableMotor();
      bot.sendMessage(chat_id_from_message, "Motor driver DISABLED.", "");
    } else if (lowerText.startsWith("turn")) {
        String valStr = text.substring(4); // Use original text for toFloat to handle potential "-"
        float degrees = valStr.toFloat();
        if (!motorGloballyEnabled) {
            bot.sendMessage(chat_id_from_message, "Motor is disabled. Use 'motoron' first.", "");
        } else {
            rotateStepperByDegrees(degrees);
            bot.sendMessage(chat_id_from_message, "Motor command: turn " + valStr + " degrees.", "");
        }
    } else if (lowerText.startsWith("step")) {
        String valStr = text.substring(4); // Use original text for toInt to handle potential "-"
        long steps = valStr.toInt();
        bool dir = HIGH; 
        if (steps < 0) {
            dir = LOW; 
            steps = -steps;
        }
        if (!motorGloballyEnabled) {
            bot.sendMessage(chat_id_from_message, "Motor is disabled. Use 'motoron' first.", "");
        } else {
            rotateStepperBySteps(steps, dir);
            bot.sendMessage(chat_id_from_message, "Motor command: step " + valStr + " steps.", "");
        }
    } else if (lowerText == "motorstatus") {
        String msg = "Motor driver is currently: ";
        msg += motorGloballyEnabled ? "ENABLED." : "DISABLED.";
        bot.sendMessage(chat_id_from_message, msg, "");
    }
  }
}

// --- Main Setup Function ---
void setup() {
  Serial.begin(115200);
  Serial.println("\nStarting ESP32 Telegram Stepper Control Bot...");

  // Initialize Stepper Motor Pins
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(EN_PIN, OUTPUT);
  disableMotor(); // Start with motor driver disabled

  // Initialize Rotary Encoder
  rotaryEncoder.begin();
  rotaryEncoder.setup(readEncoderISR);
  rotaryEncoder.setBoundaries(0, 10000, true); // Example boundaries, adjust as needed
  rotaryEncoder.setEncoderValue(0); // Initial encoder value
  rotaryEncoder.setAcceleration(0); // Disable acceleration for direct step mapping
  lastEncoderPosition = rotaryEncoder.readEncoder();
  currentEncoderPosition = lastEncoderPosition;
  Serial.println("Rotary Encoder Initialized.");
  Serial.println(motorGloballyEnabled ? "Motor driver initially ENABLED (check EN_PIN)." : "Motor driver initially DISABLED (check EN_PIN).");
  
  delay(10);

  // Connect to WiFi
  Serial.print("Connecting to Wifi SSID: "); Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  // Configure WiFiClientSecure for Telegram (ESP32 specific)
  // For ESP8266, SSL/TLS handling might differ slightly with UniversalTelegramBot or require BearSSL setup
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); 
  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: "); Serial.println(WiFi.localIP());
  bot.sendMessage(CHAT_ID, "ESP32 Stepper Bot Online!", "");
}

// --- Main Loop ---
void loop() {
  // Check for new Telegram messages
  if (millis() - bot_lasttime > BOT_MTBS) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      Serial.println("Got Telegram response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    bot_lasttime = millis();
  }

  // Check Rotary Encoder Button (to toggle motor enable state)
  if (rotaryEncoder.isEncoderButtonClicked()) {
    if (motorGloballyEnabled) {
      disableMotor();
    } else {
      enableMotor();
    }
    // Debounce or prevent rapid toggling if needed, though AiEsp32RotaryEncoder might handle some.
    while(rotaryEncoder.isEncoderButtonClicked()) delay(10); // Wait for button release
  }

  // Control Stepper Motor with Rotary Encoder (if motor is enabled)
  if (motorGloballyEnabled) {
    if (rotaryEncoder.encoderChanged()) {
      currentEncoderPosition = rotaryEncoder.readEncoder();
      bool direction = HIGH; // Default CW
      if (currentEncoderPosition > lastEncoderPosition) {
        direction = HIGH; // Or your preferred CW direction
        Serial.print("Encoder CW -> Pos: "); Serial.println(currentEncoderPosition);
        stepOnce(direction, stepMotorSpeedDelay);
      } else if (currentEncoderPosition < lastEncoderPosition) {
        direction = LOW; // Or your preferred CCW direction
        Serial.print("Encoder CCW -> Pos: "); Serial.println(currentEncoderPosition);
        stepOnce(direction, stepMotorSpeedDelay);
      }
      lastEncoderPosition = currentEncoderPosition;
    }
  }
}