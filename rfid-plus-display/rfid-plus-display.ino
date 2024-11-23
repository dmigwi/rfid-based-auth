/*!
 * @file rfid-plus-display.ino
 *
 * @section intro_sec Introduction
 *
 * This file is part rfid-plus-display package files. It is an implementation
 * of the RFID PICC that runs on a adapter customised for AVR boards.
 * It may work with other AVR boards but that cannot be guaranteed.
 *
 * @section author Author
 *
 * Written by dmigwi (Migwi Ndung'u)  @2024
 * LinkedIn: https://www.linkedin.com/in/migwi-ndungu/
 *
 *  @section license License
 *
 * BSD license, all text here must be included in any redistribution.
 */


#include "Arduino.h"
#include <LiquidCrystal.h>

namespace Settings
{
    // SERIAL_BAUD_RATE defines the data communication rate to be used during
    // serial communication.
    inline constexpr int SERIAL_BAUD_RATE {115200};

    // REFRESH_DELAY defines how long it takes to refresh the display when
    // displaying a scrolling message. 
    inline constexpr int REFRESH_DELAY {700};
};

// Display manages the relaying the status of the internal workings to the
// user via an LCD module.
class Display
{
    public:
        // Initializes the LCD library and set it to operate using 7 GPIO pins 
        // under the 4-bit mode.
        Display(int RST, int EN, int RW, int D4, int D5, int D6, int D7)
            : m_lcd {
                RST, EN, RW, // Control Pins
                D4, D5, D6, D7, // 4-bit UART data pins
            }
        {
            // set up the LCD's number of columns and rows:
            m_lcd.begin(displayCols, displayRows);
        }

        // print outputs the content on the display. It also manages scrolling 
        // if the character count is greater than the LCD can display at a go.
        void print(const String& data, int col = 0, int line = 0)
        {
            int startIndex {0};
            int endIndex {displayCols};
            int charCount { data.length() };
            
            do {
                // set the cursor to column and line arguments provided.
                m_lcd.setCursor(col, line);

                // Print a message to the LCD.
                m_lcd.print(data.substring(startIndex++, endIndex++));

                // delay for a 500ms
                delay(Settings::REFRESH_DELAY);
            } while (endIndex <= charCount);
        }

    private:
        LiquidCrystal m_lcd;

        // displayCols defines the number of columns that the LCD supports.
        const int displayCols {16};
        // displayRows the number of rows that the LCD supports.
        const int displayRows {2};
};

// Transmitter manages the Proximity Coupling Device (PCD) interface.
class Transmitter
{

};

void runSerialPassThrough()
{
    if (serialEventRun) serialEventRun();

    // SerialPassthrough code is only run when needed.
    // https://docs.arduino.cc/built-in-examples/communication/SerialPassthrough/
    #ifdef ARDUINO_AVR_LEONARDO
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
    #endif
}

int main(void)
{
	init();

#if defined(USBCON)
	USBDevice.attach();
#endif

    // LCD Pins Configuration
    const int LCD_RST {12};
    const int LCD_EN {11};
    const int LCD_RW {10};
    const int LCD_D4 {9};
    const int LCD_D5 {8};
    const int LCD_D6 {7};
    const int LCD_D7 {6};

    Serial.begin(Settings::SERIAL_BAUD_RATE);
    Serial1.begin(Settings::SERIAL_BAUD_RATE);

    Display lcdDisplay {
        LCD_RST, LCD_RW, LCD_EN, // Control Pins
        LCD_D4, LCD_D5, LCD_D6, LCD_D7 // 4-bit UART data pins
    };

    lcdDisplay.print("Hello, Warszawa!", 0, 0);

    lcdDisplay.print("The weather today is quite cold for me!", 1, 1);
    
	while(true) {
        runSerialPassThrough();
	}
        
	return 0;
}
