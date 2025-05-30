#include <Arduino.h>

#define LED_PIN 33
#define BUTTON_PIN 2

TaskHandle_t ledTaskHandle = NULL;
SemaphoreHandle_t buttonSemaphore = NULL;
volatile bool isLedPaused = false; 

void IRAM_ATTR button_ISR() {
  xSemaphoreGiveFromISR(buttonSemaphore, NULL);
}

void ledTask(void *pvParameters) {
  pinMode(LED_PIN, OUTPUT);
  bool ledState = LOW;

  Serial.println("[Core " + String(xPortGetCoreID()) + "] LED Task started.");

  for (;;) {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void buttonTask(void *pvParameters) {
  Serial.println("[Core " + String(xPortGetCoreID()) + "] Button Task started.");

  for (;;) {
    if (xSemaphoreTake(buttonSemaphore, portMAX_DELAY) == pdTRUE) {
      vTaskDelay(50 / portTICK_PERIOD_MS);

      Serial.println("[Core " + String(xPortGetCoreID()) + "] Button press detected!");

      isLedPaused = !isLedPaused;

      if (isLedPaused) {
        vTaskSuspend(ledTaskHandle);
        Serial.println("[Core " + String(xPortGetCoreID()) + "] LED Task Suspended.");
        digitalWrite(LED_PIN, LOW);
      } else {
        vTaskResume(ledTaskHandle);
        Serial.println("[Core " + String(xPortGetCoreID()) + "] LED Task Resumed.");
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("[Core " + String(xPortGetCoreID()) + "] Setup starting...");

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  buttonSemaphore = xSemaphoreCreateBinary();

  if (buttonSemaphore != NULL) {
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), button_ISR, FALLING);
    Serial.println("[Core " + String(xPortGetCoreID()) + "] Button interrupt attached.");
  } else {
    Serial.println("[Core " + String(xPortGetCoreID()) + "] Semaphore could not be created.");
  }

  xTaskCreatePinnedToCore(
      ledTask,
      "LED Task",
      2048,
      NULL,
      1,               
      &ledTaskHandle,
      0);              // Core 0

  xTaskCreatePinnedToCore(
      buttonTask,
      "Button Task",
      2048,            
      NULL,            
      2,               
      NULL,            
      1);              //  Core 1

  if (ledTaskHandle == NULL) {
      Serial.println("[Core " + String(xPortGetCoreID()) + "] LED Task could not be created.");
  }

  Serial.println("[Core " + String(xPortGetCoreID()) + "] Setup finished.");
}

void loop() {

  vTaskDelay(1000 / portTICK_PERIOD_MS); 
}