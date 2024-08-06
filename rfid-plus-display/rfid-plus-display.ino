
// Include the library code:
#include <LiquidCrystal.h>
#include <MFRC522.h>
#include <SPI.h>

#include "Arduino.h"

// LCD Pins Configuration
#define LCD_RST 12
#define LCD_EN 11
#define LCD_RW 10
#define LCD_D4 9
#define LCD_D5 8
#define LCD_D6 7
#define LCD_D7 6

/*
 * Typical pin layout used:
 * -----------------------------------------------------------------------------------------
 *             MFRC522      Arduino       Arduino   Arduino    Arduino Arduino
 *             Reader/PCD   Uno/101       Mega      Nano v3    Leonardo/Micro
 * Pro Micro Signal      Pin          Pin           Pin       Pin        Pin Pin
 * -----------------------------------------------------------------------------------------
 * RST/Reset   RST          9             5         D9         RESET/ICSP-5 RST
 * SPI SS      SDA(SS)      10            53        D10        10 10 SPI MOSI
 * MOSI         11 / ICSP-4   51        D11        ICSP-4           16 SPI MISO
 * MISO         12 / ICSP-1   50        D12        ICSP-1           14 SPI SCK
 * SCK          13 / ICSP-3   52        D13        ICSP-3           15
 *
 * SS and RST pins are configurable.
 */
#define RFID_RST A4
#define RFID_SS A5

#define SERIAL_BAUD_RATE 115200

// Functions prototypes
void serialPassthrough();

// Initialize the LCD library and set it to operate using 7 GPIO pins under
// the 4-bit mode.
LiquidCrystal lcd(LCD_RST, LCD_RW, LCD_EN,        // Control Pins
                  LCD_D4, LCD_D5, LCD_D6, LCD_D7  // 4-bit UART data pins
);

MFRC522 rc522(RFID_SS, RFID_RST);  // Create MFRC522 instance

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  Serial1.begin(SERIAL_BAUD_RATE);

  SPI.begin();       // Init SPI bus
  rc522.PCD_Init();  // Init MFRC522

  // All the MFRC522 init function to finish execution.
  delay(10);

  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  // Print a message to the LCD.
  lcd.print("Hello, Warszawa!");

  rc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader
                                    // details
  Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));
}

void loop() {
  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
  lcd.setCursor(0, 1);
  // print the number of seconds since reset:
  lcd.print(millis() / 1000);

  serialPassthrough();

  // Reset the loop if no new card present on the sensor/reader. This saves the
  // entire process when idle. Dump debug info about the card; PICC_HaltA() is
  // automatically called
  if (rc522.PICC_IsNewCardPresent() && rc522.PICC_ReadCardSerial()) {
    rc522.PICC_DumpToSerial(&(rc522.uid));
  }
}

void serialPassthrough() {
// SerialPassthrough code is only run when needed.
// https://docs.arduino.cc/built-in-examples/communication/SerialPassthrough/
#ifdef ARDUINO_AVR_LEONARDO
  // A serial passthrough is necessary because leonardo board doesn't have
  // a chip dedicated to managing the serial communication via UART protocol.
  // Copy from virtual serial line to uart and vice versa
  if (Serial.available()) {
    while (Serial.available()) {
      Serial1.write(Serial.read());
    }
  }

  if (Serial1.available()) {
    while (Serial1.available()) {
      Serial.write(Serial1.read());
    }
  }
#endif
}

char* readPICC() {}

void writePICC(char* data) {}
