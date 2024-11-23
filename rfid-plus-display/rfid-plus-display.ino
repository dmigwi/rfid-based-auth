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
#include <SPI.h>
#include <MFRC522.h>

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
    public:
        Transmitter(int SS, int RST)
            : m_rc522 {SS, RST}
        {
            SPI.begin();       // Init SPI bus
            m_rc522.PCD_Init();  // Init MFRC522

            // All the MFRC522 init function to finish execution.
            delay(10);
        }

        // isNewCardDetected returns true for the new card detected and their
        // respective serial numbers can be read.
        bool isNewCardDetected()
        {
            return m_rc522.PICC_IsNewCardPresent() && m_rc522.PICC_ReadCardSerial();
        }

        // readPICC reads the contents of a given Proximity Inductive Coupling Card (PICC/NFC Card)
        char* readPICC() {}

        void writePICC(const char* data) {}

    private:
        MFRC522 m_rc522;

};

void runSerialPassthrough()
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

    /*
    * Typical pin layout used:
    * -----------------------------------------------------------------------------------------
    *             MFRC522      Arduino       Arduino   Arduino    Arduino Arduino
    *             Reader/PCD   Uno/101       Mega      Nano v3    Leonardo/Micro
    * Pro Micro Signal          Pin          Pin           Pin       Pin        Pin Pin
    * -----------------------------------------------------------------------------------------
    * RST/Reset    RST           9         5          D9            RESET/ICSP-5 RST
    * SPI SS       SDA(SS)       10        53         D10           10 10 SPI MOSI
    * MOSI         11 / ICSP-4   51        D11        ICSP-4        16 SPI MISO
    * MISO         12 / ICSP-1   50        D12        ICSP-1        14 SPI SCK
    * SCK          13 / ICSP-3   52        D13        ICSP-3        15
    *
    * SS and RST pins are configurable.
    */

    // PCD Transmitter Pins 
    const int RFID_RST {22}; // A4 Pin
    const int RFID_SS {23};  // A5 Pin

    Serial.begin(Settings::SERIAL_BAUD_RATE);
    Serial1.begin(Settings::SERIAL_BAUD_RATE);

    Display lcdDisplay {
        LCD_RST, LCD_RW, LCD_EN, // Control Pins
        LCD_D4, LCD_D5, LCD_D6, LCD_D7 // 4-bit UART data pins
    };

    lcdDisplay.print("Hello, Warszawa!", 0, 0);

    lcdDisplay.print("The weather today is quite cold for me!", 1, 1);

    Transmitter rc522 {RFID_SS, RFID_RST};
    
	while(true) {
        if (rc522.isNewCardDetected())
        {
            // TODO: Set the Card Reading status to the display.
            const char* cardData {rc522.readPICC()};

            // TODO: Indicate success or failure of the card reading operation.

            // TODO: Set the card's serial writting status to the WIFI module.
            Serial.print(cardData);

            // TODO: on feedback recieved, set the receiving message status.

            // TODO: set the card writting status.
            rc522.writePICC(cardData);
        }
        runSerialPassthrough();
	}
        
	return 0;
}
