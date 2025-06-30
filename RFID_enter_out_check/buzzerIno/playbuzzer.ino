void playBuzzer(int status) {


  if (status == 1) { 
    tone(BUZZER_PIN, BEEP_FREQUENCY, 150); 
    delay(180);
    tone(BUZZER_PIN, BEEP_FREQUENCY, 400); 
  } 
  else if (status == 0) { 
    tone(BUZZER_PIN, BEEP_FREQUENCY, 100); 
    delay(120);
    tone(BUZZER_PIN, BEEP_FREQUENCY, 100); 
    delay(120);
    tone(BUZZER_PIN, BEEP_FREQUENCY, 100);
  } 
  else if (status == 2) { 
    tone(BUZZER_PIN, BEEP_FREQUENCY, 1000); 
    delay(1100); 
    tone(BUZZER_PIN, BEEP_FREQUENCY, 1000);
  }
}