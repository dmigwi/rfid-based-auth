/*!
 * @file transmitter.cpp
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

#include "transmitter.h"

// Assigns the global variable an initial value of false.
volatile bool onInterrupt {false};

///////////////////////////////////////////////////
// Display Class Members
//////////////////////////////////////////////////

// Display class constructor initializes the LCD library and set it to
// operate using 7 GPIO pins under the 4-bit mode.
Display::Display(
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
void Display::setStatusMsg(char* data, bool displayNow = false)
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
void Display::setDetailsMsg(char* data, bool displayNow = false)
{
    // If content mismatch in length, clean up the screen before printing
    // the new content.
    m_isClearScreen  = ((strlen(data) != strlen(m_info[m_detailsIndex])));

    m_info[m_detailsIndex] = data;

    // Triggers immediate screen update if true.
    if (displayNow)
        printScreen();
}

// printScreen refreshes the display so that messages longer than max characters
// supported can be scrolled from right to left.
void Display::printScreen()
{
    if (m_isClearScreen)
        m_lcd.clear(); // clean up the screen first.

    if (strlen(m_info[m_statusIndex]) > 0)
        print(m_statusIndex, 0, 0); // print on Row 1

    if (strlen(m_info[m_detailsIndex]) > 0)
        print(m_detailsIndex, 0, 1); // print on Row 2
}

///////////////////////////////////////////////////
// Transmitter Class Members
//////////////////////////////////////////////////

// Transmitter constructor.
Transmitter::Transmitter(
    uint8_t RFID_SS, uint8_t RFID_RST, // RFID control pins
    uint8_t LCD_RST, uint8_t LCD_RW, uint8_t LCD_EN,  // LCD Control Pins
    uint8_t LCD_D4, uint8_t LCD_D5, uint8_t LCD_D6, uint8_t LCD_D7 // LCD 4-bit UART data pins
    )
    : Display(LCD_RST, LCD_RW, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7),
        m_rc522 {RFID_SS, RFID_RST}
{ 
    // Display the bootup welcome message.
    setState(Transmitter::BootUp);
    setDetailsMsg((char*)"The weather today is quite cold for me!");
    printScreen();

    SPI.begin();            // Init SPI bus.
    m_rc522.PCD_Init();     // Init MFRC522 Library.

    // Allow all the MFRC522 init function to finish execution.
    delay(Settings::REFRESH_DELAY);
}

 // isNewCardDetected returns true for the new card detected and their
// respective serial numbers can be read.
bool Transmitter::isNewCardDetected()
{
    return m_rc522.PICC_IsNewCardPresent() && m_rc522.PICC_ReadCardSerial();
}

// print outputs the content on the display. It also manages scrolling
// if the character count is greater than the LCD can display at a go.
void Transmitter::print(uint8_t index, uint8_t col, uint8_t line)
{
    const char* data {getRowData(index)};

    do{
        // set the cursor to column and line arguments provided.
        setScreenCursor(col, line);

        // Print a message to the LCD. Move the cursor using pointer maths
        printScreen(data++);

        activateTransmission(); // Trigger IRQ interrupt checking.

        // delay
        delay(Settings::REFRESH_DELAY);

    // If Interrupt is activated, hand over the control back to the 
    // caller so that the interrupt can be processed soonest possible.
    }while (strlen(data) >= maxColumns && !onInterrupt);
}

// stateToStatus returns the string version of each state translated to
// its corresponding status message.  
char* Transmitter::stateToStatus(Transmitter::MachineState& state) const
{
    switch (state)
    {
        case BootUp:
            return (char*)"Hello, Warszawa!"; // Welcome Message.
        case StandBy:
            return (char*)"Tag Waiting ..."; // Waiting for a tag.
        case ReadTag:
            return (char*)"Tag Reading ..."; // Reading the tag.
        case Network:
            return (char*)"Connections ..."; // Network Connection.
        case WriteTag:
            return (char*)"Tag Writing ..."; // Writing the tag.
        default:
            return (char*)"--Unknown!--";
    }
}

// setState updates the state of the device and the status message
// that appears on the display.
void Transmitter::setState(Transmitter::MachineState state)
{
    m_state = state; 

    setStatusMsg(stateToStatus(state));
}


// readPICC reads the contents of a given Proximity Inductive Coupling Card (PICC/NFC Card)
char* Transmitter::readPICC() 
{
    // A Tag has been detected thus the machines state is updated to read tag state.
    setState(Transmitter::ReadTag);

    char* data {};
    // Stage 1: Activate the Card (Request + Anticollision + Select)
    //  - Card is activated and UID is retrieved. Card is ready for next operations.
    // Stage 2: Authentication
    // - Three pass authentication sequence handled by the reader automatically
    // Stage 3: Read the Card block/sector contents.
    return data;
}

// networkConn establishes Connection to the wifi Module via a serial communication.
// The WIFI module then connects to the validation server where the PICC
// card data is validated.
char* Transmitter::networkConn(const char* cardData)
{
    // With data read from the tag, connection to the validation server is established.
    setState(Transmitter::Network);

    Serial.print(cardData);
}

// writePICC writes the provided content to the PICC. 
void Transmitter::writePICC(const char* ) 
{
    // With data returned from the validation server, PICC can be written.
    setState(Transmitter::WriteTag);


}

// ccleanUpAfterCardOps undertake reset operation back to the standby
// state after the read, network connection and write operation
// on a PICC completes.
void Transmitter::cleanUpAfterCardOps()
{
    // Reset the machine state to standy by indicating that the device is ready
    // to handle another PICC selected.
    setState(Transmitter::StandBy);
    
    // Set interrupt as successfully handled.
    resetInterrupt();

    // disable the interrupt till it is activated again.
    onInterrupt = false;

    // Move the PICC from Active state to Idle after processing is done.
    m_rc522.PICC_HaltA();
}

// handleDetectedCard on detecting an NFC card within the field, an interrupt
// is triggered which forces reading and writting of the necessary data to
// the card to be done as a matter of urgency.
void Transmitter::handleDetectedCard()
{
    if (isNewCardDetected())
    {
        setDetailsMsg((char*)"Holy Crap it works, Hurray!!!!");

        // TODO: Set the Card Reading status to the display.
        const char* cardData {readPICC()};

        // TODO: Indicate success or failure of the card reading operation.

        // TODO: Set the card's serial writting status to the WIFI module.
        networkConn(cardData);

        // TODO: on feedback recieved, set the receiving message status.

        // TODO: set the card writting status.
        writePICC(cardData);

        // Handle clean up after the card operations.
        cleanUpAfterCardOps();
    }
}
