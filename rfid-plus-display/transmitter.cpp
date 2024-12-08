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
    byte RST, byte RW, byte EN,  // Control Pins
    byte D4, byte D5, byte D6, byte D7 // 4-bit UART data pins
)
    : m_lcd {RST, RW, EN,  D4, D5, D6, D7}
{
    // set up the LCD's number of columns and rows:
    m_lcd.begin(maxColumns, maxRows);
}

// setStatusMsg set the status message that is to be displayed on Row 1.
// This message is mostly concise with clear message and doesn't require
// scrolling. The displayNow option defaults to false unless specified
// as true.
void Display::setStatusMsg(char* data, bool displayNow=false)
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
// may be scrollable. The displayNow option defaults to true unless specified
// as false.
void Display::setDetailsMsg(char* data, bool displayNow=true)
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
    byte RFID_SS, byte RFID_RST, // RFID control pins
    byte LCD_RST, byte LCD_RW, byte LCD_EN,  // LCD Control Pins
    byte LCD_D4, byte LCD_D5, byte LCD_D6, byte LCD_D7 // LCD 4-bit UART data pins
    )
    : Display(LCD_RST, LCD_RW, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7),
        m_rc522 {RFID_SS, RFID_RST}
{ 
    // Display the bootup welcome message.
    setState(Transmitter::BootUp);
    setDetailsMsg((char*)"The weather today is quite cold for me!");

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
void Transmitter::print(byte index, byte col, byte line)
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

// setPICCAuthKey generates the authentication keys from the card UID.
// This makes it safer since revealing the authentication keys of a 
// single card does not affect overall system security
void Transmitter::setPICCAuthKey()
{
    // A valid UID can have 4, 7 or 10 bytes. This implies that if a valid UID
    // was identified, a minimum of 4 and a max of the 6 bytes in the init key
    // might be XORed to generate the ultimate key to be used for authentication.
    for (int i {0}; i < MFRC522::MF_KEY_SIZE; ++i)
        m_PICCKey.keyByte[i] = (Settings::initKey[i] ^ m_rc522.uid.uidByte[i]);
}

// attemptAuthentication uses the various keys available to detect which
// one enable read/write operations of the card.
// Blocks 0 - 3 are not considered as they could be hold the cards
// configuration used in other sectors.
Transmitter::BlockAuth Transmitter::attemptAuthentication(MFRC522::MIFARE_Key key)
{
    Transmitter::BlockAuth auth;
    auth.status = MFRC522::STATUS_ERROR; // set default status to error.

    // Attempt to authenticate using the Key passed.
    for (byte blockNo {4}; blockNo <= Settings::maxBlockNo; ++blockNo)
    {
        if ((blockNo + 1) % 4 == 0) // Ignore access bit configuration block.
            continue;

        auth.status = m_rc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockNo, &key, &(m_rc522.uid));
        if (auth.status == MFRC522::STATUS_OK)
        {
            auth.blockNo = blockNo;
            auth.authKey = key;
            break; // Key used is successful in authentication.
        }
        else 
        {
            // Must reselect and activate the card again so that we can try more
            // blocks according to: http://arduino.stackexchange.com/a/14316
            if (!isNewCardDetected())
                break; // If false, the card reactivation failed.
        }
    }
    return auth;
}

// readPICC reads the contents of a given Proximity Inductive Coupling Card (PICC/NFC Card)
Transmitter::UserData& Transmitter::readPICC() 
{
    UserData data {};
    data.status = MFRC522::STATUS_ERROR; // set default status to error.
    m_blockAuth.status = MFRC522::STATUS_ERROR; // set default status to error.

    // Stage 1: Activate the Card (Request + Anticollision + Select)
    //  - Card is already activated and UID retrieved. Card is ready for next operations.

    // A Tag has been detected thus the machines state is updated to read tag state.
    setState(Transmitter::ReadTag);

    setDetailsMsg((char*)"Initiating authentication to confirm key validity!");

    // Stage 2: Authentication
    // - Three pass authentication sequence handled by the reader automatically
    
    // sets the authentication key based on the selected card UID. 
    setPICCAuthKey();

    // Initiate authentication first using the UID based Key before the default 
    // keys can be tested.
    BlockAuth tempVal {attemptAuthentication(m_PICCKey)};

    if (tempVal.status != MFRC522::STATUS_OK)
    {
        // The card has not been reprogrammed before with its UID based key.
        for (byte i {0}; i < Settings::keysCount; ++i)
        {
            tempVal = attemptAuthentication(Settings::defaultPICCKeys[i]);
            if (tempVal.status == MFRC522::STATUS_OK)
                break; // A key the works has been identified.
        }
    }

    if (tempVal.status != MFRC522::STATUS_OK)
    {
        setDetailsMsg((char*)"Error: Key validity confirmation failed. Try another tag!");
        return data;
    }

    // Copy the validated auth data now into blockAuth.
    m_blockAuth.status = tempVal.status;
    m_blockAuth.blockNo = tempVal.blockNo;
    // Undertake a deep copy of the key byte array.
    memcpy(m_blockAuth.authKey.keyByte, tempVal.authKey.keyByte, MFRC522::MF_KEY_SIZE);

    // Stage 3: Read the Card block contents.
    // - The data to be read is supposed to of size dataSize.
    byte blocksToRead {Settings::dataSize/Settings::blockSize};
    byte lastValidBlock {m_blockAuth.blockNo + blocksToRead + 1}; // ignore access bits block

    if (lastValidBlock < Settings::maxBlockNo)
        setDetailsMsg((char*)"Initiating tag reading to extract data!");
    else
    {
        setDetailsMsg((char*)"Error: tag blocks are almost full. Try another tag!");
        return data;
    }

    // Reads 16 bytes (+ 2 bytes CRC_A) from the active PICC.
    byte buffer[Settings::blockSize + 2]; 
    byte byteCount = sizeof(buffer);

    byte startBlock {0};
    byte addr {m_blockAuth.blockNo-1}; // Minus one because increment happens before usage.

    while (startBlock < blocksToRead)
    {
        if ((++addr + 1) % 4 == 0) // Ignore access bit configuration block.
            continue;

        data.status = m_rc522.MIFARE_Read(addr, buffer, &byteCount);
        if (data.status != MFRC522::STATUS_OK)
            break;

        // copy the read data with 2 bytes of CRC_A bytes.
        memcpy(data.readData+(startBlock* Settings::blockSize), buffer, Settings::blockSize);
        ++startBlock; // Only increment if a data block is read.
    }

    if (data.status == MFRC522::STATUS_OK)
        setDetailsMsg((char*)"Tag reading operation was successful!");
    else
        setDetailsMsg((char*)"Error: Reading the tag failed. Try another tag!");

    return data;
}

// networkConn establishes Connection to the wifi Module via a serial communication.
// The WIFI module then connects to the validation server where the PICC
// card data is validated.
Transmitter::UserData& Transmitter::networkConn(byte* cardData)
{
    UserData data {};
    data.status = MFRC522::STATUS_ERROR; // set default status to error.

    // With data read from the tag, connection to the validation server is established.
    setState(Transmitter::Network);

    setDetailsMsg((char*)"Initiating network connection!");

    // Handle Serial transmission to and from the WIFI module.
    // Data inform of bytes to be sent to/from the WIFI module should be in the
    // following format:
    // [1 byte for UID size (4/7/10)] + [10 bytes card's UID] + [64 bytes Actual data]
    // In total 75 bytes should be transmitted via the serial communication.

    byte byteCount = Settings::dataSize + 10 + 1;
    byte txData[byteCount];
    memcpy(txData, cardData, Settings::dataSize); // copy card actual data.
    memcpy(txData+Settings::dataSize, m_rc522.uid.uidByte, 10); // copy card uid.
    txData[Settings::dataSize+10] = m_rc522.uid.size; // copy card uid size.

    // Write the data into the serial transmission.
    Serial.write(txData, byteCount);
    // Set the serial timeout to be 2000 milliseconds.
    Serial.setTimeout(2000);

    // read the bytes sent back from the WIFI module.
    Serial.readBytes(txData, byteCount);

    byte uidBuffer[10];
    memcpy(uidBuffer, txData+Settings::dataSize, 10); // copy card uid.
    int match = memcmp(m_rc522.uid.uidByte, uidBuffer, 10);

    // byte previously used from size is used as status code.
    // Code 0 indicates success otherwise anything else indicates failure.
    if (match == 0 && txData[Settings::dataSize+10] == 0)
    {
        setDetailsMsg((char*)"Network connection was successful!");
        data.status = MFRC522::STATUS_OK; // update status
        memcpy(data.readData, txData, Settings::dataSize); // update the new card data
    }
    else
        setDetailsMsg((char*)"Error: Network connection failed. Try another tag!");
    return data;
}

// writePICC writes the provided content to the PICC. 
void Transmitter::writePICC(byte* cardData)
{
    // With data returned from the validation server, PICC can be written.
    setState(Transmitter::WriteTag);

    setDetailsMsg((char*)"Initiating tag writting operation!");

    MFRC522::StatusCode status;
    byte blocksToRead {Settings::dataSize/Settings::blockSize};
    byte buffer[Settings::blockSize];

    byte startBlock {0};
    byte addr {m_blockAuth.blockNo-1}; // Minus one because increment happens before usage.

    while (startBlock < blocksToRead)
    {
        if ((++addr + 1) % 4 == 0) // Ignore access bit configuration block.
            continue;

        memcpy(cardData+(startBlock* Settings::blockSize), buffer, Settings::blockSize);
        ++startBlock; // Only increment if a data block is read.

        status = m_rc522.MIFARE_Write(addr, buffer, Settings::blockSize);
        if (status != MFRC522::STATUS_OK)
            break;
    }

    if (status == MFRC522::STATUS_OK)
        setDetailsMsg((char*)"Tag writting operation was successful!");
    else
        setDetailsMsg((char*)"Error: Writing the tag failed. Try another tag!");
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

    // Stop encryption on PCD allowing new communication to be initiated with other PICCs.
    m_rc522.PCD_StopCrypto1();
}

// handleDetectedCard on detecting an NFC card within the field, an interrupt
// is triggered which forces reading and writting of the necessary data to
// the card to be done as a matter of urgency.
void Transmitter::handleDetectedCard()
{
    if (isNewCardDetected())
    {
        UserData cardData {readPICC()};

        // Only send the cards data in the reading operation was successful.
        if (cardData.status == MFRC522::STATUS_OK)
        {
            const UserData tempVal { networkConn(cardData.readData) };
            // copy the returned net data into card data.
            cardData.status = tempVal.status;
            memcpy(cardData.readData, tempVal.readData, Settings::dataSize);
        }

        // Only write the card data if the network operation was successful.
        if (cardData.status == MFRC522::STATUS_OK)
        {
            writePICC(cardData.readData);
        }

        // Handle clean up after the card operations.
        cleanUpAfterCardOps();
    }
}
