#include <AiEsp32RotaryEncoder.h>


#define STEP_PIN 23
#define DIR_PIN 22
#define EN_PIN 21


#define ROTARY_ENCODER_A_PIN 14       // CLK pin
#define ROTARY_ENCODER_B_PIN 33       // DT pin
#define ROTARY_ENCODER_BUTTON_PIN 27  // SW pin
#define ROTARY_ENCODER_STEPS 4        // Encoder'ınızın bir tıkı için kaç sinyal ürettiği


int hizGecikmesi = 2000; 
long mevcutEncoderPozisyonu = 0;
long sonEncoderPozisyonu = 0;
volatile boolean motorAktif = true; 

AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, -1, ROTARY_ENCODER_STEPS);

void IRAM_ATTR readEncoderISR() {
  rotaryEncoder.readEncoder_ISR();
}

void setup() {
  Serial.begin(115200);
  Serial.println("Rotary Encoder ile Step Motor Kontrolü");

  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, HIGH);

  rotaryEncoder.begin();
  rotaryEncoder.setup(readEncoderISR);
  rotaryEncoder.setBoundaries(0, 1000, true);
  rotaryEncoder.setEncoderValue(500);
  rotaryEncoder.setAcceleration(0);

  sonEncoderPozisyonu = rotaryEncoder.readEncoder();
  mevcutEncoderPozisyonu = sonEncoderPozisyonu;
  Serial.println("Motor AKTIF!");
}

void tekAdimAt(int hiz) {
  digitalWrite(STEP_PIN, HIGH);
  delayMicroseconds(hiz);
  digitalWrite(STEP_PIN, LOW);
  delayMicroseconds(hiz);
}

void loop() {
  if (rotaryEncoder.isEncoderButtonClicked()) {
    motorAktif = !motorAktif;  

    if (motorAktif) {
      digitalWrite(EN_PIN, LOW);  
      Serial.println("Motor AKTIF Edildi!");
    } else {
      digitalWrite(EN_PIN, HIGH);  
      Serial.println("Motor DURDURULDU!");
    }
    delay(50);
  }

 
  if (motorAktif) {
    if (rotaryEncoder.encoderChanged()) {
      mevcutEncoderPozisyonu = rotaryEncoder.readEncoder();

      if (mevcutEncoderPozisyonu > sonEncoderPozisyonu) {
        digitalWrite(DIR_PIN, HIGH);
        tekAdimAt(hizGecikmesi);
        Serial.print("Pozisyon: ");
        Serial.println(mevcutEncoderPozisyonu);
      } else if (mevcutEncoderPozisyonu < sonEncoderPozisyonu) {
        digitalWrite(DIR_PIN, LOW);
        tekAdimAt(hizGecikmesi);
        Serial.print("Pozisyon: ");
        Serial.println(mevcutEncoderPozisyonu);
      }
      sonEncoderPozisyonu = mevcutEncoderPozisyonu;
    }
  }
}