#include <LiquidCrystal_I2C.h>


const int LCD_I2C_ADDR = 0x23; 

const int LCD_COLS = 16;
const int LCD_ROWS = 2;

const int I2C_SDA_PIN = 13;
const int I2C_SCL_PIN = 14;

LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);

void setup() {
  Serial.begin(115200);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  
  lcd.init();
  lcd.backlight();

  Serial.println("LCD Testi Basladi.");

  lcd.setCursor(0, 0);
  lcd.print("Hello World!");

  lcd.setCursor(0, 1);
  lcd.print("Test Done!");
}

void loop() {
}