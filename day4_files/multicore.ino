#include <Arduino.h>

TaskHandle_t Task1;

void Task1code( void * pvParameters ){
  Serial.print("Task1 running on core ");
  Serial.println(xPortGetCoreID());

  for(;;){
    Serial.println("hello from core 0!");
    vTaskDelay(1000 / portTICK_PERIOD_MS); 
  } 
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.print("setup() running on core ");
  Serial.println(xPortGetCoreID());

  xTaskCreatePinnedToCore(
                    Task1code,
                    "Task1",
                    10000,
                    NULL,
                    1,
                    &Task1,
                    0);
                    
  delay(500); 
}

void loop() {
  Serial.print("loop() running on core ");
  Serial.println(xPortGetCoreID());
  Serial.println("Hello from Core 1 (loop)!");

  delay(1500); 
}