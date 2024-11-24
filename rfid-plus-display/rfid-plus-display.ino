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

#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal.h>

namespace Settings
{
    // SERIAL_BAUD_RATE defines the data communication rate to be used during
    // serial communication.
    inline constexpr int SERIAL_BAUD_RATE {115200};

    // REFRESH_DELAY defines how long it takes to refresh the display when
    // displaying a scrolling message. 
    inline constexpr int REFRESH_DELAY {700};

    // Transmitter defines in this namespace because the interrupt handler method
    // cannot be a class method. 
    Transmitter rfid;
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

        // setMainMessage sets the main message that is to be displayed on Row 1.
        // This message is mostly concise with clear message and doesn't require
        // scrolling.
        void setMainMessage(const String& data)
        {
            m_message = data;
        }

        // setStatusMessage set the status message that is to be displayed on Row 2.
        // This message is usually a longer explanation of the main message and
        // may be scrollable.
        void setStatusMessage(const String& data)
        {
            m_status = data;
        }

        // print refreshes the display so that messages longer than max characters
        // supported can be scrolled from right to left.
        void print()
        {
            if (!m_message.length() > 0)
                print(m_message, 0, 0); // print on Row 1

            if (!m_status.length() > 0)
                print(m_status, 0, 1); // print on Row 2
        }
    
    protected:
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

                // delay
                delay(Settings::REFRESH_DELAY);
            } while (endIndex <= charCount);
        }

    private:
        LiquidCrystal m_lcd;
        String m_message {""};
        String m_status{""};

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
        char* readPICC() 
        {
            // Stage 1: Activate the Card (Request + Anticollision + Select)
            //  - Card is activated and UID is retrieved. Card is ready for next operations.
            // Stage 2: Authentication
            // - Three pass authentication sequence handled by the reader automatically
            // Stage 3: Read the Card block/sector contents.
        }

        void writePICC(const char* data) {}

    private:
        MFRC522 m_rc522;
};

// handleDetectedCard on detecting an NFC card within the field, an interrupt
// is triggered which forces reading and writting of the necessary data to
// the card to be done as a matter of urgency.
void handleDetectedCard()
{
    if (Settings::rfid.isNewCardDetected())
    {
        // TODO: Set the Card Reading status to the display.
        const char* cardData {Settings::rfid.readPICC()};

        // TODO: Indicate success or failure of the card reading operation.

        // TODO: Set the card's serial writting status to the WIFI module.
        Serial.print(cardData);

        // TODO: on feedback recieved, set the receiving message status.

        // TODO: set the card writting status.
        Settings::rfid.writePICC(cardData);
    }
}

void runSerialPassthrough()
{
    #ifdef ARDUINO_AVR_LEONARDO
    // SerialPassthrough code is only run when needed.
    // https://docs.arduino.cc/built-in-examples/communication/SerialPassthrough/
    // A serial passthrough is necessary because leonardo board doesn't have
    // a chip dedicated to managing the serial communication via UART protocol.
    // Copy from virtual serial line to uart and vice versa
    if (Serial.available()) {
        while(Serial.available())
            Serial1.write(Serial.read());
    }

    if (Serial1.available()) {
        while(Serial1.available())
            Serial.write(Serial1.read());
    }
    #endif

    if (serialEventRun) serialEventRun();
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

    // PCD Transmitter Pins 
    const int RFID_RST {22}; // A4 Pin
    const int RFID_SS {23};  // A5 Pin
    const int RFID_IRQ {2}; // Interrupt pin

    // setup the IRQ pin
    pinMode(RFID_IRQ, INPUT_PULLUP);

    Serial.begin(Settings::SERIAL_BAUD_RATE);
    Serial1.begin(Settings::SERIAL_BAUD_RATE);

    Display lcdDisplay {
        LCD_RST, LCD_RW, LCD_EN, // Control Pins
        LCD_D4, LCD_D5, LCD_D6, LCD_D7 // 4-bit UART data pins
    };

    lcdDisplay.setMainMessage("Hello, Warszawa!");
    lcdDisplay.setStatusMessage("The weather today is quite cold for me!");

    Settings::rfid = Transmitter {RFID_SS, RFID_RST};

    // Activate the RFID interrupt on the RISING mode.
    attachInterrupt(digitalPinToInterrupt(RFID_IRQ), handleDetectedCard, RISING);
    
	while(true) {
        // refresh the display.
        lcdDisplay.print();
       
        runSerialPassthrough();
        delay(Settings::REFRESH_DELAY);
	}
        
	return 0;
}
