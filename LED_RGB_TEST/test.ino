#include <FastLED.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

\\#define NUM_PIXELS           69
#define LED_PIN              19
#define BUTTON_1             32
#define MATRIX_CLK_PIN       33
#define MATRIX_DATA_PIN      14
#define MATRIX_CS_PIN        27
#define MATRIX_HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MATRIX_MAX_DEVICES   4

// --- TEST EDİLECEK RENK SIRASI --- // RGB GBR BRG BGR GRB
#define COLOR_ORDER BRG

// --- NESNELER ---
CRGB leds[NUM_PIXELS];
MD_Parola P(MATRIX_HARDWARE_TYPE, MATRIX_DATA_PIN, MATRIX_CLK_PIN, MATRIX_CS_PIN, MATRIX_MAX_DEVICES);
int testState = 0; // 0: Sönük, 1: Kırmızı, 2: Yeşil, 3: Mavi

void setup() {
  Serial.begin(115200);
  delay(1000); // Monitörü açmak için zaman

  // FastLED'i test edilecek renk sırasıyla başlat
  FastLED.addLeds<WS2811, LED_PIN, COLOR_ORDER>(leds, NUM_PIXELS);
  FastLED.setBrightness(60);
  fill_solid(leds, NUM_PIXELS, CRGB::Black);
  FastLED.show();

  // Matrix ekranı başlat
  P.begin();
  P.setIntensity(4);
  P.displayText("TEST", PA_CENTER, 0, 0, PA_NO_EFFECT, PA_NO_EFFECT);

  // Butonu ayarla
  pinMode(BUTTON_1, INPUT_PULLUP);
  
  Serial.println("Renk Testi Basladi. Lutfen 1. butona basin.");
}

void loop() {
  if (P.displayAnimate()) {
    P.displayReset();
  }

  // Butona basıldığında bir sonraki test rengine geç
  if (digitalRead(BUTTON_1) == LOW) {
    testState++;
    if (testState > 3) {
      testState = 0; // Başa dön
    }

    switch (testState) {
      case 0: // SÖNÜK
        Serial.println("--------------------");
        Serial.println("LED'ler kapatildi.");
        P.displayText("TEST", PA_CENTER, 0, 0, PA_NO_EFFECT, PA_NO_EFFECT);
        fill_solid(leds, NUM_PIXELS, CRGB::Black);
        break;
      case 1: // KIRMIZI
        Serial.println("Istenen Renk: KIRMIZI");
        P.displayText("RED", PA_CENTER, 0, 0, PA_NO_EFFECT, PA_NO_EFFECT);
        fill_solid(leds, NUM_PIXELS, CRGB::Red);
        break;
      case 2: // YEŞİL
        Serial.println("Istenen Renk: YESIL");
        P.displayText("GREEN", PA_CENTER, 0, 0, PA_NO_EFFECT, PA_NO_EFFECT);
        fill_solid(leds, NUM_PIXELS, CRGB::Green);
        break;
      case 3: // MAVİ
        Serial.println("Istenen Renk: MAVI");
        P.displayText("BLUE", PA_CENTER, 0, 0, PA_NO_EFFECT, PA_NO_EFFECT);
        fill_solid(leds, NUM_PIXELS, CRGB::Blue);
        break;
    }
    FastLED.show();
    delay(500); // Butonun birden fazla basılmasını engelle
  }
}