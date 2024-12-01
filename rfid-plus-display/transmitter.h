/*!
 * @file transmitter.h
 *
 * @section intro_sec Introduction
 *
 * This file is part rfid-plus-display package files. It is an implementation
 * of the RFID PCD (Proximity Coupled Device) that runs on a adapter customised 
 * mainly for AVR boards.
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

#ifndef __RFID_TRANSMITTER__
#define __RFID_TRANSMITTER__

#include "Arduino.h"

#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal.h>

// onInterrupt is declared as a global variable that is set to true once an
// interrupt by the RFID module is recorded.
extern volatile bool onInterrupt;

namespace Settings
{
    // SERIAL_BAUD_RATE defines the data communication rate to be used during
    // serial communication.
    constexpr long int SERIAL_BAUD_RATE {115200};

    // REFRESH_DELAY defines how long it takes to refresh the display when
    // displaying a scrolling message. 
    constexpr int REFRESH_DELAY {700};
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
        );

        // setStatusMsg set the status message that is to be displayed on Row 1.
        // This message is mostly concise with clear message and doesn't require
        // scrolling.
        void setStatusMsg(char* data, bool displayNow = false);

        // setDetailsMsg sets the details message that is to be displayed on Row 2.
        // This message is usually a longer explanation of the status message and
        // may be scrollable.
        void setDetailsMsg(char* data, bool displayNow = false);

        // printScreen refreshes the display so that messages longer than max characters
        // supported can be scrolled from right to left.
        void printScreen();

        // printScreen writes content on the actual display window.
        void printScreen(const char* info)  { m_lcd.print(info);  }

        // getRowData returns the row data once provided with the respective index.
        const char* getRowData(uint8_t index) const
        {
            return m_info[min(index, maxRows-1)];
        }

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
            );

        // isNewCardDetected returns true for the new card detected and their
        // respective serial numbers can be read.
        bool isNewCardDetected()
        {
            return m_rc522.PICC_IsNewCardPresent() && m_rc522.PICC_ReadCardSerial();
        }

        // print outputs the content on the display. It also manages scrolling 
        // if the character count is greater than the LCD can display at a go.
        void print(uint8_t index, uint8_t col, uint8_t line) override;

        // readPICC reads the contents of a given Proximity Inductive Coupling Card (PICC/NFC Card)
        char* readPICC();

        // writePICC writes the provided content to the PICC. 
        void writePICC(const char* );

        // handleDetectedCard on detecting an NFC card within the field, an interrupt
        // is triggered which forces reading and writting of the necessary data to
        // the card to be done as a matter of urgency.
        void handleDetectedCard();

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
            if (onInterrupt) return;

            m_rc522.PCD_WriteRegister(MFRC522::FIFODataReg, MFRC522::PICC_CMD_REQA);
            m_rc522.PCD_WriteRegister(MFRC522::CommandReg, MFRC522::PCD_Transceive);
            m_rc522.PCD_WriteRegister(MFRC522::BitFramingReg, initDataTransmission);
        }

    private:
        MFRC522 m_rc522;

        // activateIRQ defines the register value to be set activating the
        // IRQ pin as an interrupt. Sets interrupts to be active Low and only 
        // allows the receiver interrupt request (RxIRq).
        static const int activateIRQ {0xA0};
        // handledInterrupt defines the register value to be set indicating that
        // the interrupt has been handled.
        static const int handledInterrupt {0x7F};
        // initDataTransmission defines the register value to be set when initiating
        // data transmission via bit framing command with no last byte.
        static const int initDataTransmission {0x87};
};

#endif