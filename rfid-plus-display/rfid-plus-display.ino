
// include the library code:
#include "Arduino.h"
#include <LiquidCrystal.h>


// LCD Pins Configuration
#define LCD_RST 12
#define LCD_EN 11
#define LCD_RW 10
#define LCD_D4 9
#define LCD_D5 8
#define LCD_D6 7
#define LCD_D7 6


// Initialize the LCD library and set it to operate using 7 GPIO pins under
// the 4-bit mode.
LiquidCrystal lcd(
  LCD_RST, LCD_RW, LCD_EN, // Control Pins
  LCD_D4, LCD_D5, LCD_D6, LCD_D7 // 4-bit UART data pins
);

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);

   // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  // Print a message to the LCD.
  lcd.print("Hello, Warszawa!");
}

void loop() {
  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
  lcd.setCursor(0, 1);
  // print the number of seconds since reset:
  lcd.print(millis() / 1000);


  // A serial passthrough is necessary because leonardo board doesn't have
  // a chip dedicated to managing the serial communication via UART protocol.
  // Copy from virtual serial line to uart and vice versa
  if (Serial.available()) {
    while(Serial.available()){
      Serial1.write(Serial.read());
    }
  }

  if (Serial1.available()) {
    while(Serial1.available()) {
      Serial.write(Serial1.read());
    }
  }
}


