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
    constexpr long int SERIAL_BAUD_RATE {115200};

    // REFRESH_DELAY defines how long it takes to refresh the display when
    // displaying a scrolling message. 
    constexpr int REFRESH_DELAY {700};

    // isInterruptFlag is set to true once an interrupt by the RFID module is
    // recorded.
    volatile bool isInterruptFlag {false};

    // volatile int counter {0}; // for tests
};

// Display manages the relaying the status of the internal workings to the
// user via an LCD module.
class Display
{
    public:
        // Initializes the LCD library and set it to operate using 7 GPIO pins 
        // under the 4-bit mode.
        Display(
            uint8_t RST, uint8_t RW, uint8_t EN,  // Control Pins
            uint8_t D4, uint8_t D5, uint8_t D6, uint8_t D7 // 4-bit UART data pins
        )
            : m_lcd {RST, RW, EN,  D4, D5, D6, D7}
        {
            // set up the LCD's number of columns and rows:
            m_lcd.begin(maxColumns, maxRows);
        }

        // setStatusMsg set the status message that is to be displayed on Row 1.
        // This message is mostly concise with clear message and doesn't require
        // scrolling.
        void setStatusMsg(char* data, bool displayNow = false)
        {
            // If content mismatch in length, clean up the screen before printing
            // the new content.
            m_isClearScreen = ((strlen(data) != strlen(m_info[m_statusIndex])));

            m_info[m_statusIndex] = data;

            // Triggers immediate screen update if true.
            if (displayNow)
                printScreen();
        }

        // setDetailsMsg sets the details message that is to be displayed on Row 2.
        // This message is usually a longer explanation of the status message and
        // may be scrollable.
        void setDetailsMsg(char* data, bool displayNow = false)
        {
            // If content mismatch in length, clean up the screen before printing
            // the new content.
            m_isClearScreen  = ((strlen(data) != strlen(m_info[m_detailsIndex])));

            m_info[m_detailsIndex] = data;

            // Triggers immediate screen update if true.
            if (displayNow)
                printScreen();
        }
        // // Only needed for testing. 
        // void setTemp(int data)
        // {
        //     String(data).toCharArray(m_info[m_detailsIndex], maxColumns);
        //         printScreen();
        // }

        // printScreen refreshes the display so that messages longer than max characters
        // supported can be scrolled from right to left.
        void printScreen()
        {
            if (m_isClearScreen)
                m_lcd.clear(); // clean up the screen first.

            if (strlen(m_info[m_statusIndex]) > 0)
                print(m_statusIndex, 0, 0); // print on Row 1

            if (strlen(m_info[m_detailsIndex]) > 0)
                print(m_detailsIndex, 0, 1); // print on Row 2
        }

        //  getRowData returns the row data once provided with the respective index.
        const char* getRowData(uint8_t index) const
        {
            return m_info[min(index, maxRows-1)];
        }

        // printScreen writes content on the actual display window.
        void printScreen(const char* info)  { m_lcd.print(info);  }

        // setScreenCursor set the position of the cursor. It should be used
        // before any content is written on the screen.
        void setScreenCursor(uint8_t col, uint8_t line)  
        {  
            m_lcd.setCursor(col, line);
        }

        // print is made virtual to allow customisation in the Transmitter class. 
        virtual void print(uint8_t /*index*/, uint8_t /*col*/, uint8_t /*line*/){}
    
    protected:
        // maxColumns defines the number of columns that the LCD supports.
        // (Top to Bottom)
        static const uint8_t maxColumns {16};

        // maxRows the number of rows that the LCD supports. (Left to Right)
        static const uint8_t maxRows {2};

        // m_statusIndex defines the index of the message displayed on Row 1.
        static const uint8_t m_statusIndex {0};

        // m_detailsIndex defines the index of the scrollable explanation message
        // displayed in the Row 2.
        static const uint8_t m_detailsIndex {1};

        // isClearScreen indicates if the display contents should be clean up the
        // current contents on display before display new stuff.
        bool m_isClearScreen {false};

    private:
        LiquidCrystal m_lcd;

        // m_info holds the current messages to be displayed on both 
        char* m_info[maxRows]{};
};

// Transmitter manages the Proximity Coupling Device (PCD) interface.
class Transmitter: public Display
{
    public:
        Transmitter(
            uint8_t RFID_SS, uint8_t RFID_RST, // RFID control pins
            uint8_t LCD_RST, uint8_t LCD_RW, uint8_t LCD_EN,  // LCD Control Pins
            uint8_t LCD_D4, uint8_t LCD_D5, uint8_t LCD_D6, uint8_t LCD_D7 // LCD 4-bit UART data pins
            )
            : Display(LCD_RST, LCD_RW, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7),
                m_rc522 {RFID_SS, RFID_RST}
        {
            SPI.begin();       // Init SPI bus
            m_rc522.PCD_Init();  // Init MFRC522

            // Allow all the MFRC522 init function to finish execution.
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
            char* data {};
            // Stage 1: Activate the Card (Request + Anticollision + Select)
            //  - Card is activated and UID is retrieved. Card is ready for next operations.
            // Stage 2: Authentication
            // - Three pass authentication sequence handled by the reader automatically
            // Stage 3: Read the Card block/sector contents.
            return data;
        }

        void writePICC(const char* ) {}

        // handleDetectedCard on detecting an NFC card within the field, an interrupt
        // is triggered which forces reading and writting of the necessary data to
        // the card to be done as a matter of urgency.
        void handleDetectedCard()
        {
            if (isNewCardDetected())
            {
                // TODO: Set the Card Reading status to the display.
                const char* cardData {readPICC()};

                // TODO: Indicate success or failure of the card reading operation.

                // TODO: Set the card's serial writting status to the WIFI module.
                Serial.print(cardData);

                // TODO: on feedback recieved, set the receiving message status.

                // TODO: set the card writting status.
                writePICC(cardData);

                setStatusMsg((char*)"New Card!!", true);
            }
            setDetailsMsg((char*)"Hurray!!!! Holy Crap it works.");
            // setTemp(Settings::counter);

            // Set interrupt as successfully handled.
            resetInterrupt();
        }

        // resetInterrupt clears the pending interrupt bits after being resolved.
        // Enables the module to detect new interrupts.
        void resetInterrupt()
        {
            m_rc522.PCD_WriteRegister(MFRC522::ComIrqReg, handledInterrupt);
        }

        // enableInterrupts activates interrupts in IRQ pin.
        void enableInterrupts()
        {
            m_rc522.PCD_WriteRegister(MFRC522::ComIEnReg, activateIRQ);
        }

        // activateTransmission triggers the recieving block within the rfid to send
        // the an interrupt once detected.
        void activateTransmission()
        {
            // Wait for the current interrupt to be handled before triggering another
            // one.
            if (Settings::isInterruptFlag)
                return;

            m_rc522.PCD_WriteRegister(MFRC522::FIFODataReg, MFRC522::PICC_CMD_REQA);
            m_rc522.PCD_WriteRegister(MFRC522::CommandReg, MFRC522::PCD_Transceive);
            m_rc522.PCD_WriteRegister(MFRC522::BitFramingReg, initDataTransmission);
        }

        // print outputs the content on the display. It also manages scrolling 
        // if the character count is greater than the LCD can display at a go.
        void print(uint8_t index, uint8_t col, uint8_t line) override
        {
            const char* data {getRowData(index)};

            do{
                // set the cursor to column and line arguments provided.
                setScreenCursor(col, line);

                // Print a message to the LCD. Move the cursor using pointer maths
                printScreen(data++);
                Serial.println(data);

                activateTransmission(); // Trigger IRQ interrupt checking.

                // delay
                delay(Settings::REFRESH_DELAY);

            // If Interrupt is activated, hand over the control back to the 
            // caller so that the interrupt can be processed soonest possible.
            }while (strlen(data) >= maxColumns && !Settings::isInterruptFlag);
        }

    private:
        MFRC522 m_rc522;

        // activateIRQ defines the register value to be set activating the
        // IRQ pin as an interrupt. Sets interrupts to be active Low and only 
        // allows the receiver interrupt request (RxIRq).
        static const int activateIRQ {0xA0};
        // handledInterrupt defines the register value to be set indicating that
        // the interrupt has been handled.
        static const int handledInterrupt {0x80};
        // initDataTransmission defines the register value to be set when initiating
        // data transmission via bit framing command with no last byte.
        static const int initDataTransmission {0x87};
};

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

// handleInterrupt sets that an interrupt has been detected allowing it to be 
// responded to immediately.
void handleInterrupt() 
{  
    Settings::isInterruptFlag = true;
    // ++Settings::counter; 
    // Serial.println(Settings::counter);
}

// Main function.
int main(void)
{
	init();

#if defined(USBCON)
	USBDevice.attach();
#endif

    // LCD Pins Configuration
    const uint8_t LCD_RST {12};
    const uint8_t LCD_EN {11};
    const uint8_t LCD_RW {10};
    const uint8_t LCD_D4 {9};
    const uint8_t LCD_D5 {8};
    const uint8_t LCD_D6 {7};
    const uint8_t LCD_D7 {6};

    // PCD Transmitter Pins 
    const uint8_t RFID_RST {22}; // A4 Pin
    const uint8_t RFID_SS {23};  // A5 Pin
    const uint8_t RFID_IRQ {2}; // Interrupt pin

    Transmitter rfid {
            RFID_SS, RFID_RST,              // RC522 control Pins
            LCD_RST, LCD_RW, LCD_EN,        // LCD Control Pins
            LCD_D4, LCD_D5, LCD_D6, LCD_D7  // LCD 4-bit UART data pins
        };

    // setup the IRQ pin
    pinMode(RFID_IRQ, INPUT_PULLUP);

    // Setup the RFID interrupt handler on the CHANGE mode.
    attachInterrupt(digitalPinToInterrupt(RFID_IRQ), handleInterrupt, CHANGE);

    // Clear initial false positive interrupt.
    do { ; } while(!Settings::isInterruptFlag);

    rfid.enableInterrupts(); // enable interrupts
    Settings::isInterruptFlag = false;

    Serial.begin(Settings::SERIAL_BAUD_RATE);
    Serial1.begin(Settings::SERIAL_BAUD_RATE);

    rfid.setStatusMsg((char*)"Hello, Warszawa!");
    rfid.setDetailsMsg((char*)"The weather today is quite cold for me!");
    
	for(;;) {
        // Print to the display
        rfid.printScreen();

        // Handle the interrupt if it has been detected.
        if (Settings::isInterruptFlag)
        {
            rfid.handleDetectedCard();
            // disable the interrupt till it is activated again.
            Settings::isInterruptFlag = false;
        }
       
        runSerialPassthrough();

        // RFID module is requested to transmit it data so that the microcontroller
        // can confirm if there is an interrupt to be handled. 
        rfid.activateTransmission();

        delay(Settings::REFRESH_DELAY);
	}
        
	return 0;
}
