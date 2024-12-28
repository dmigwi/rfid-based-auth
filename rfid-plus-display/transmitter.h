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

#include "commonRFID.h"

// IS_TRUST_ORG flag is used to indicate that the current PCD mode allows a trust
// organization to over write blank memory space if no previous Trust Key exists.
// #define IS_TRUST_ORG

// onInterrupt is declared as a global variable that is set to true once an
// interrupt by the RFID module is recorded.
extern volatile bool onInterrupt;

namespace Settings
{
    // Import the common settings configurations here.
    using namespace CommonRFID;

    // TODO: A more easier approach to update it should be implemented.
    constexpr byte DEVICE_ID[] {0xef, 0x12, 0x34, 0x56, 0xab, 0xcd, 0xef, 0x12};

    // keysCount defines the number of default keys to attempt authentication
    // with in new cards.
    constexpr int keysCount {9};

    #ifdef IS_TRUST_ORG
    // defaultPICCKeyAs defines the common/default keys used to access blocks.
    // THIS KEY SHOULD ONLY BE USED DURING THE Trust Organization MODE ONLY!.
    // https://github.com/nfc-tools/libnfc/blob/0e8cd450e1ad467b845399d55f6322a39c072b44/utils/nfc-mfclassic.c#L82-L92
    static const MFRC522::MIFARE_Key defaultPICCKeyAs[keysCount]
    {
        {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, // FF FF FF FF FF FF = factory default
        {0xd3, 0xf7, 0xd3, 0xf7, 0xd3, 0xf7}, // D3 F7 D3 F7 D3 F7
        {0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5}, // A0 A1 A2 A3 A4 A5
        {0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5}, // B0 B1 B2 B3 B4 B5
        {0x4d, 0x3a, 0x99, 0xc3, 0x51, 0xdd}, // 4D 3A 99 C3 51 DD
        {0x1a, 0x98, 0x2c, 0x7e, 0x45, 0x9a}, // 1A 98 2C 7E 45 9A
        {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}, // AA BB CC DD EE FF
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // 00 00 00 00 00 00
        {0xab, 0xcd, 0xef, 0x12, 0x34, 0x56}, // AB CD EF 12 34 56
    };
    #endif

    // KeyA defines the default hardcoded key that has read only to read block 2
    // contents in a trust key sector during the first pass of the
    // authentication process. i.e (DA 91 E7 A4 3B 42)
    static const MFRC522::MIFARE_Key KeyA = {0xDA, 0x91, 0xE7, 0xA4, 0x3B, 0x42};

     #ifdef IS_TRUST_ORG
    constexpr byte accessBitsCount {3};

    // AccessBits configure the read and write permissions of each block in a sector.
    // Sector K - Sector to be used to store a Trust Key.
    //     block 0 – data block        |    Read: Only KeyB, Write: Only KeyB
    //     block 1 – data block        |    Read: Only KeyB, Write: Only KeyB
    //     block 2 – data block        |    Read: KeyA/KeyB, Write: KeyA/KeyB
    //     block 3 – sector trailer    |    Read: Never,     Write: Only KeyB
    static const byte AccessBits[accessBitsCount] = {0x4B, 0x44, 0xBB};
     #endif

    // Since all Mifare classics NFC chip have consistent a block organisations
    // between sectors 0 and 16. The read/write operation will only consider the
    // blocks in those sectors.
    // TODO: For Mifare Classics 4k Support reading more than the 16th block.
    // Block 0 continues Manufacturers data I.e. Serial Number
    // Going forward the every 4th block holds access bits for the specific sector
    // i.e. the next 3 blocks following.
    // Block 63 is the last block in the 16 sectors to be considered.
    const byte maxBlockNo {63};

    // sectorBlocks defines the number of blocks in each sector and are organised
    // as follows for the mifare classics family.
    //     sector 0:
    //          block 0 – manufacturer data (read only)
    //          block 1 – data block
    //          block 2 – data block
    //          block 3 – sector trailer
    //      sector 1:
    //          block 4 – data block
    //          block 5 – data block
    //          block 6 – data block
    //          block 7 – sector trailer
    //        ...
    //      sector 16:
    //          block 60 – data block
    //          block 61 – data block
    //          block 62 – data block
    //          block 63 – sector trailer
    constexpr byte sectorBlocks {4};
};

// Display manages the relaying the status of the internal workings to the
// user via an LCD module.
class Display
{
    public:
        typedef struct
        {
           char* text; // display text
           byte index; // tracks the cursor position.
        } Msg;

        // Initializes the LCD library and set it to operate using 7 GPIO pins
        // under the 4-bit mode.
        Display(
            byte RST, byte RW, byte EN,  // Control Pins
            byte D4, byte D5, byte D6, byte D7 // 4-bit UART data pins
        );

        // setStatusMsg set the status message that is to be displayed on Row 1.
        // This message is mostly concise with clear message and doesn't require
        // scrolling.
        void setStatusMsg(char* data, bool displayNow, bool isClear);

        // setDetailsMsg sets the details message that is to be displayed on Row 2.
        // This message is usually a longer explanation of the status message and
        // may be scrollable.
        void setDetailsMsg(char* data, bool displayNow);

        // printScreen refreshes the display so that messages longer than max characters
        // supported can be scrolled from right to left.
        void printScreen();

        // print outputs the content on the display. It also manages scrolling
        // if the character count is greater than the LCD can display at a go.
        void print(Msg &data, byte col, byte line);

    protected:
        // maxColumns defines the number of columns that the LCD supports.
        // (Top to Bottom)
        static const byte maxColumns {16};

        // maxRows the number of rows that the LCD supports. (Left to Right)
        static const byte maxRows {2};

    private:
        LiquidCrystal m_lcd;

        // m_statusMsg defines the text and cursor position of the message
        // displayed on Row 1. Allocated less bytes as less/no scrolling is needed.
        Msg m_statusMsg{new char[30]{}, 0};

        // m_detailsMsg defines the text and cursor position of the message
        // displayed on Row 2.
        Msg m_detailsMsg{new char[80]{}, 0};
};

// Transmitter manages the Proximity Coupling Device (PCD) interface.
class Transmitter: public Display
{
    public:
        // MachineState describes the various phase the devices oscillates in
        // between during its normal operation.
        enum MachineState {
            // BootUp State is the initial state set on device power On.
            BootUp,
            // Loading State is when the WIFI configuration is being setup.
            Loading,
            // StandBy State is the Idle State when waiting for tag(s) to read/write.
            StandBy,
            // ReadTag State is set on tag detection where authentication and
            // Blocks/Sectors reading is carried out.
            ReadTag,
            // Network State is set when the data read is sent to an
            // validation server.
            Network,
            // WriteTag State is set when the validation server sends data to be
            // written into the tag.
            WriteTag,

            // Unknown State is set to indicate the undefined state the machine is
            // currently in.
            Unknown,
        };

        // BlockAuth defines data to be returned once a successsful authentication
        // has been established on a card.
        typedef struct
        {
            byte block0Addr;  // Holds the first data block address in each sector.
            bool isCardNew; // True only if the Uid based key is not set as the default.
            MFRC522::StatusCode status;
            MFRC522::MIFARE_Key authKeyA;
            byte block2Data[Settings::blockSize]; // After auth, block2 contents are read.
        } BlockAuth;


        // UserData defines the type of data expected after a successful read or write.
        typedef struct
        {
            MFRC522::StatusCode status;
            byte readData[Settings::TrustKeySize];
        } UserData;


        Transmitter(
            byte RFID_SS, byte RFID_RST, // RFID control pins
            byte LCD_RST, byte LCD_RW, byte LCD_EN,  // LCD Control Pins
            byte LCD_D4, byte LCD_D5, byte LCD_D6, byte LCD_D7 // LCD 4-bit UART data pins
            );

        // isNewCardDetected returns true for the new card detected and their
        // respective serial numbers can be read.
        bool isNewCardDetected();

        // setPICCAuthKeyB generates the KeyB authentication bytes from XORing a
        // combination of secretKey, TagUid and KeyA. KeyB = (secretKey ⨁ KeyA ⨁ TagUid)
        // secretKey is a server provided 6 bytes key unique to every trust key.
        void setPICCAuthKeyB(byte* secretKey);

        // attemptBlock2Auth loops through all the supported sectors
        // attempting to authenticate the Block 2 part of it. It trys to find
        // which of the hardcoded default KeyAs is currently supported by the tag.
        void attemptBlock2Auth(BlockAuth& auth, MFRC522::MIFARE_Key key);

        // setUidBasedKey if the card uses non-uid based key for  authentication,
        // it is replace with a Uid based which is quicker and safer to use.
        MFRC522::StatusCode setUidBasedKey();

        // readPICC reads the contents of a given Proximity Inductive Coupling Card (PICC/NFC Card)
        void readPICC();

        // networkConn establishes Connection to the wifi Module via a serial communication.
        // The WIFI module then connects to the validation server where the PICC
        // card data is validated.
        void networkConn();

        // writePICC writes the provided content to the PICC.
        void writePICC();

        // ccleanUpAfterCardOps undertake reset operation back to the standby
        // state after the read, network connection and write operation
        // on a PICC completes.
        void cleanUpAfterCardOps();

        // handleDetectedCard on detecting an NFC card within the field, an interrupt
        // is triggered which forces reading and writting of the necessary data to
        // the card to be done as a matter of urgency.
        void handleDetectedCard();

        // stateToStatus returns the string version of each state translated to
        // its corresponding status message.
        char* stateToStatus(MachineState& state) const;

        // setState updates the state of the device.
        void setState(MachineState state);

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

        MachineState m_state {BootUp};

        BlockAuth m_blockAuth{};

        UserData m_cardData{};

        // m_PiccKeyB defines the key that is generated from the card's uid.
        // It is more safer and easier to use than that the other default keys
        // it is unique for every tag and cannot be computed the trust organization's
        // secret key.
        MFRC522::MIFARE_Key m_PiccKeyB;

        // activateIRQ defines the register value to be set activating the
        // IRQ pin as an interrupt. Sets interrupts to be active Low and only
        // allows the receiver interrupt request (RxIRq).
        static const byte activateIRQ {0xA0};
        // handledInterrupt defines the register value to be set indicating that
        // the interrupt has been handled.
        static const byte handledInterrupt {0x7F};
        // initDataTransmission defines the register value to be set when initiating
        // data transmission via bit framing command with no last byte.
        static const byte initDataTransmission {0x87};
};

#endif
