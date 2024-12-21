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

// dumpBytes dumps the buts on the serial output.
void dumpBytes(byte* data, byte count)
{
     for (byte i = 0; i < count; i++) {
            Serial.print(data[i], HEX);
            Serial.print(F(" "));
        }
        Serial.println();
}

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
void Display::setStatusMsg(char* data, bool displayNow = false)
{
    // If content mismatch, clean up the screen before printing the new content.
    m_statusMsg.text = data;
    m_statusMsg.index = 0;

    // Triggers immediate screen update if true.
    if (displayNow)
        printScreen();
}

// setDetailsMsg sets the details message that is to be displayed on Row 2.
// This message is usually a longer explanation of the status message and
// may be scrollable. The displayNow option defaults to true unless specified
// as false.
// NB: Text longer than 41 characters distorts the row 1 characters.
void Display::setDetailsMsg(char* data, bool displayNow = true)
{
    // If content mismatch, clean up the screen before printing the new content.
    m_detailsMsg.text = data;
    m_detailsMsg.index = 0;

    // Triggers immediate screen update if true.
    if (displayNow)
        printScreen();

    // Serial.println(data);
}

// printScreen refreshes the display so that messages longer than max characters
// supported can be scrolled from right to left.
void Display::printScreen()
{
    if (strlen(m_statusMsg.text) > 0)
        print(m_statusMsg, 0, 0); // print on Row 1

    if (strlen(m_detailsMsg.text) > 0)
        print(m_detailsMsg, 0, 1); // print on Row 2
}

// print outputs the content on the display. It also manages scrolling
// if the character count is greater than the LCD can display at a go.
void Display::print(Msg &data, byte col, byte line)
{
    // set the cursor to column and line arguments provided.
    m_lcd.setCursor(col, line);
    // Print a message to the LCD. Move the cursor using pointer maths
    m_lcd.print(data.text + data.index);

    if (strlen(data.text) > maxColumns)
        if ((strlen(data.text) - ++data.index) <  maxColumns)
            data.index = 0; // reset cursor position if it exceeds max characters. 
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
    setDetailsMsg((char*)"The weather today is too cold for me (:!");

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
            return (char*)"WiFi Conn ..."; // Network Connection.
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

// setPICCAuthKeyB generates the KeyB authentication bytes from XORing a
// combination; of secretKey, TagUid and KeyA. KeyB = (secretKey ⨁ KeyA ⨁ TagUid)
// secretKey is a server provided 6 bytes to increase difficulty in KeyB duplication.
void Transmitter::setPICCAuthKeyB(byte* secretKey)
{
    // A valid Uid can have 4, 7 or 10 bytes. If the Uid has less 4 bytes, the
    // remaining bytes will be defaulted to zero each.
    byte TagUid[MFRC522::MF_KEY_SIZE] = {0, 0, 0, 0, 0, 0};
    byte bytesToCopy {
        (m_rc522.uid.size < static_cast<byte>(MFRC522::MF_KEY_SIZE)) ?
        m_rc522.uid.size :
        static_cast<byte>(MFRC522::MF_KEY_SIZE)
    };
    memcpy(TagUid, m_rc522.uid.uidByte, bytesToCopy); // copy tag uid bytes.

    for (int i {0}; i < MFRC522::MF_KEY_SIZE; ++i)
        m_PiccKeyB.keyByte[i] = (secretKey[i] ^ Settings::KeyA.keyByte[i] ^ TagUid[i]);
}

// attemptBlock2Auth loops through all the supported sectors
// attempting to authenticate the Block 2 part of it. If successful contents
// of the block 2 address are read. It trys to find which of the hardcoded
// default list of KeyA keys is currently supported by the tag.
void Transmitter::attemptBlock2Auth(Transmitter::BlockAuth& auth, MFRC522::MIFARE_Key key)
{
    auth.status = MFRC522::STATUS_ERROR; // set default status to error.

    // Block 0-3 belongs to sector 0 are not considered. The first valid block 2
    // considered appears in sector 1 and at intervals of Settings::sectorBlocks.
    byte block2Addr {6}; // block 2 in sector 1.

    // Reads 16 bytes (+ 2 bytes CRC_A) from the active PICC.
    byte buffer[Settings::blockSize + 2];
    byte byteCount = sizeof(buffer);

    for (; block2Addr <= Settings::maxBlockNo; block2Addr += Settings::sectorBlocks)
    {
        MFRC522::PICC_Command keyType = MFRC522::PICC_CMD_MF_AUTH_KEY_A;
        auth.status = m_rc522.PCD_Authenticate(keyType, block2Addr, &key, &(m_rc522.uid));
        if (auth.status == MFRC522::STATUS_OK)
        {
            // Authentication is successful on this block 2 address. Now
            // compute the block 0 address in the current sector.
            auth.block0Addr = block2Addr - 2;
            // Serial.print(F("Data block chosen :"));
            // Serial.println(auth.block0Addr);
            // Serial.print(F("Sector block chosen :"));
            // Serial.println(block2Addr+1);

            // Reads Contents of Block2
            auth.status = m_rc522.MIFARE_Read(block2Addr, buffer, &byteCount);
            if (auth.status == MFRC522::STATUS_OK)
                break;
            else
                isNewCardDetected(); // reactivate the tag after previous op failure.

            byteCount = sizeof(buffer); // reset the buffer counter.
        }
        else
        {
            // Must reselect and activate the card again so that we can try more
            // sector blocks according to: http://arduino.stackexchange.com/a/14316
            if (!isNewCardDetected())
                break; // If false, the card reactivation failed.
        }
    }

    if (auth.status == MFRC522::STATUS_OK)
    {
        // Deep copy the validated keyA.
       memcpy(auth.authKeyA.keyByte, key.keyByte, MFRC522::MF_KEY_SIZE);

        // If the current keyA matches the default KeyA, then the card
        // must have been used before, therefore not new.
        auth.isCardNew = (memcmp(key.keyByte, Settings::KeyA.keyByte, MFRC522::MF_KEY_SIZE) != 0);

        // copy the read data without the 2 bytes of CRC_A.
        memcpy(auth.block2Data, buffer, Settings::blockSize);
    }
}

// readPICC reads the contents of a given Proximity Inductive Coupling Card (PICC/NFC Card)
Transmitter::UserData Transmitter::readPICC()
{
    UserData data {};
    data.status = MFRC522::STATUS_ERROR; // set default status to error.
    m_blockAuth.status = MFRC522::STATUS_ERROR; // set default status to error.

    // Stage 1: Activate the Card (Request + Anticollision + Select)
    //  - Card is already activated and UID retrieved. Card is ready for next operations.

    // A Tag has been detected thus the machines state is updated to read tag state.
    setState(Transmitter::ReadTag);
    setDetailsMsg((char*)"Init authentication to validate key!");

    // Stage 2: Attempt Authentication using KeyA and read block 2 address contents
    // if successful.
   
    // Initiate authentication first using the default main KeyA
    attemptBlock2Auth(m_blockAuth, Settings::KeyA);
    // Serial.println("Auth using main keyA ");

    if (m_blockAuth.status != MFRC522::STATUS_OK)
    {
        // The card has not been reprogrammed before with its UID based key.
        for (byte i {0}; i < Settings::keysCount; ++i)
        {
            // Serial.print("Auth using default keyA: ");
            // Serial.println(i);
            attemptBlock2Auth(m_blockAuth, Settings::defaultPICCKeyAs[i]);
            if (m_blockAuth.status == MFRC522::STATUS_OK)
                break; // A key that works has been identified, break the loop.
        }
    }

    if (m_blockAuth.status != MFRC522::STATUS_OK)
    {
        setDetailsMsg((char*)"KeyA validity failed. Try another tag!");
        return data;
    }

    // Serial.println(F(" Block 2 contents! ")); 
    // dumpBytes(m_blockAuth.block2Data, Settings::blockSize);

    // Stage 3: Send the block 2 Contents to the trust organization for validation.
    // - Use Serial transmission to send the data to and from the WIFI module.

    //  Send block 2 address data.
    Serial.write(m_blockAuth.block2Data, Settings::blockSize);

    // Request the secret key sent from the trust organization. A default of 
    // 0x00 bytes is preset.
    byte secretKey[MFRC522::MF_KEY_SIZE] = {0, 0, 0, 0, 0, 0};

    /*
    // Handle narrowing conversion
    byte bytesRead { static_cast<byte>(Serial.readBytes(secretKey, MFRC522::MF_KEY_SIZE))};
    if (bytesRead != MFRC522::MF_KEY_SIZE)
    {
        setDetailsMsg((char*)"Error: Fetching the Secret Key failed. Try another tag!");
        return data;
    }
    */

    // This is only for tests: copy simulated secret key bytes.
    byte tempReadBytes[MFRC522::MF_KEY_SIZE] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
    memcpy(secretKey, tempReadBytes, MFRC522::MF_KEY_SIZE);
    
    // KeyB needs to be computed using the successfully read secret key.
    // KeyA only has read-only permissions to block 2 address while KeyB has both
    // read and write permissions to the whole sector.
    setPICCAuthKeyB(secretKey);

    // Stage 4: Read the Card block contents.
    // - The data to be read is supposed to of size dataSize.
    byte blocksToRead {Settings::dataSize/Settings::blockSize};
    byte lastValidBlock {(byte)(m_blockAuth.block0Addr + blocksToRead)};

    if (lastValidBlock <= Settings::maxBlockNo)
        setDetailsMsg((char*)"Initiating tag reading to extract data!");
    else
    {
        setDetailsMsg((char*)"Tag blocks are full. Try another tag!");
        return data;
    }

    // Reads 16 bytes (+ 2 bytes CRC_A) from the active PICC.
    byte buffer[Settings::blockSize + 2];
    byte byteCount = sizeof(buffer);

    byte startBlock {0};
    byte addr {m_blockAuth.block0Addr};

    // Serial.println(F(" Tag Serial contents! ")); 
    // dumpBytes(m_rc522.uid.uidByte, m_rc522.uid.size);

    // Serial.println(F(" KeyB contents! ")); 
    // dumpBytes(m_PiccKeyB.keyByte, MFRC522::MF_KEY_SIZE);

    for (;startBlock < blocksToRead && addr < Settings::maxBlockNo; ++addr)
    {
        if ((addr + 1) % Settings::sectorBlocks == 0)
            continue;   // Ignore access bit configuration block.

        // authenticate each block before attempting a read operation.
        // If the card is new, use either KeyA or KeyB as they are similar otherwise use Tag Specific KeyB.
        data.status = m_rc522.PCD_Authenticate(
            MFRC522::PICC_CMD_MF_AUTH_KEY_B,                                // authenticate with Key B
            addr,                                                           // data block number
            (m_blockAuth.isCardNew ? &m_blockAuth.authKeyA : &m_PiccKeyB),  // KeyB already preset
            &(m_rc522.uid)                                                  // Selected Card Uid
        );

        if (data.status != MFRC522::STATUS_OK)
            break; // break on block authentication failure

        data.status = m_rc522.MIFARE_Read(addr, buffer, &byteCount);
        if (data.status != MFRC522::STATUS_OK)
            break; // break on read authentication failure

        byteCount = sizeof(buffer); // reset the buffer count.

        // copy the read data without the 2 bytes of CRC_A.
        memcpy(data.readData+(startBlock* Settings::blockSize), buffer, Settings::blockSize);
        ++startBlock; // Only increment if a data block is read.
    }

    if (data.status == MFRC522::STATUS_OK)
        setDetailsMsg((char*)"Tag reading operation was successful!");
    else
        setDetailsMsg((char*)"Reading the tag failed. Try another tag!");

    // Serial.println(F(" TrustKey contents reading! ")); 
    // dumpBytes(data.readData, Settings::dataSize);

    return data;
}

// networkConn establishes Connection to the wifi Module via a serial communication.
// The WIFI module then connects to the validation server where the PICC
// card data is validated.
void Transmitter::networkConn(Transmitter::UserData &cardData)
{

    cardData.status = MFRC522::STATUS_ERROR; // set default status to error.

    /*// for tests only
    byte testData[] = {
       0xDA, 0x91, 0xE7, 0xA4, 0x3B, 0x42, 0x4B, 0x44, 0xBB, 0x00, 0x8A, 0xBB, 0xBC, 0x90, 0xA1, 0xFE,
       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
       0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0xab, 0xcd, 0xef, 0x12,
    };

    cardData.status = MFRC522::STATUS_OK; // update status
    memcpy(cardData.readData, testData, Settings::dataSize);
    return;*/

    // With data read from the tag, connection to the validation server is established.
    setState(Transmitter::Network);
    setDetailsMsg((char*)"Initiating network connection!");

    // Handle Serial transmission to and from the WIFI module.
    // Data inform of bytes to be sent to/from the WIFI module should be in the
    // following format:
    // [1 byte for UID size (4/7/10)] + [10 bytes card's UID] + [48 bytes Actual data]
    // In total 59 bytes should be transmitted via the serial communication.

    size_t byteCount = Settings::dataSize + 10 + 1;
    byte txData[byteCount];
    memcpy(txData, cardData.readData, Settings::dataSize); // copy card actual data.
    memcpy(txData+Settings::dataSize, m_rc522.uid.uidByte, 10); // copy card uid.
    txData[Settings::dataSize+10] = m_rc522.uid.size; // copy card uid size.

    // Write the data into the serial transmission.
    Serial.write(txData, byteCount);

    // read the bytes sent back from the WIFI module.
    size_t bytesRead = Serial.readBytes(txData, byteCount);

    byte uidBuffer[10];
    memcpy(uidBuffer, txData+Settings::dataSize, 10); // copy card uid.
    int match = memcmp(m_rc522.uid.uidByte, uidBuffer, 10);

    // For a successful Network Data read:
    // 1. Bytes read must match the expected.
    // 2. Card uid returned must match the existing one.
    // 3. Byte previously used as size is now used as status code. Status 0 indicates
    // success otherwise anything else indicates failure.
    if (byteCount == bytesRead && match == 0 && txData[Settings::dataSize+10] == 0)
    {
        setDetailsMsg((char*)"Network connection was successful!");
        cardData.status = MFRC522::STATUS_OK; // update status
        memcpy(cardData.readData, txData, Settings::dataSize); // update the new card data
        return;
    }
    setDetailsMsg((char*)"Network connection failed!");
}

// writePICC writes the provided content to the PICC.
void Transmitter::writePICC(Transmitter::UserData &cardData)
{
    // With data returned from the validation server, PICC can be written.
    setState(Transmitter::WriteTag);
    setDetailsMsg((char*)"Initiating tag writing operation!");

    byte blocksToRead {Settings::dataSize/Settings::blockSize};
    byte buffer[Settings::blockSize];

    byte startBlock {0};
    byte addr {m_blockAuth.block0Addr};

    for (;startBlock < blocksToRead && addr < Settings::maxBlockNo; ++addr)
    {
        if ((addr + 1) % Settings::sectorBlocks == 0)
            continue; // Ignore access bit configuration block.

        memcpy(cardData.readData+(startBlock* Settings::blockSize), buffer, Settings::blockSize);
        ++startBlock; // Only increment if a data block is read.

        cardData.status = m_rc522.MIFARE_Write(addr, buffer, Settings::blockSize);
        if (cardData.status != MFRC522::STATUS_OK)
            break;
    }

    // Serial.println(F(" TrustKey contents writing! ")); 
    // dumpBytes(cardData.readData, Settings::dataSize);

    if (cardData.status == MFRC522::STATUS_OK)
        setDetailsMsg((char*)"Tag writing operation was successful!");
    else
        setDetailsMsg((char*)"Writing the tag failed. Try another tag!");
}

// cleanUpAfterCardOps undertake reset operation back to the standby
// state after the read, network connection and write operation
// on a PICC completes.
void Transmitter::cleanUpAfterCardOps()
{
    delay(Settings::AUTH_DELAY); // Delay before making another PICC selection.

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

    m_blockAuth = BlockAuth{}; // reset the block authentication information.
}

// handleDetectedCard on detecting an NFC card within the field, an interrupt
// is triggered which forces reading and writing of the necessary data to
// the card to be done as a matter of urgency.
void Transmitter::handleDetectedCard()
{
    if (isNewCardDetected())
    {
        UserData cardData {readPICC()};

        // Only send the cards data in the reading operation was successful.
        if (cardData.status == MFRC522::STATUS_OK)
            networkConn(cardData);

        // Only write the card data if the network operation was successful.
        if (cardData.status == MFRC522::STATUS_OK)
            writePICC(cardData);

         if (cardData.status == MFRC522::STATUS_OK)
            cardData.status = setUidBasedKey(); // Upgrade Key if the card is new.

        // Handle clean up after the card operations.
        cleanUpAfterCardOps();
    }
}

// setUidBasedKey replaces the non-uid base key with a Uid based which is
// quicker and safer to use. This is done on the cards detected as new.
MFRC522::StatusCode Transmitter::setUidBasedKey()
{
    MFRC522::StatusCode status = MFRC522::STATUS_ERROR;

    if (m_blockAuth.isCardNew)
    {
        // card must be new otherwise KeyA and KeyB won't match as specified
        // in the tag's transport configuration from the factory.
        
        // +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
        // | ------------- KEY A -------------- | - ACCESS BITS -  | -GP- | -------------- KEY B ------------- |
        // | 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF | 0xFF, 0x07, 0x80 | 0x00 | 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF |
        // +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
        // Above diagram describes the default transport configuration of the 
        // Mifare classics family of tags sector block.
        // Default KeyA = 0xFF FF FF FF FF FFh.
        // Default KeyB = 0xFF FF FF FF FF FFh.
        byte keyBuffer[Settings::blockSize] = {
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // KeyA
            0xFF, 0x07, 0x80,                   // Access bits (Read/Write using Key A only)
            0x00,                               // General Purpose byte
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // KeyB
        };

        // Set KeyA.
        memcpy(keyBuffer, Settings::KeyA.keyByte, MFRC522::MF_KEY_SIZE);

        // Set Access Bits
        memcpy(keyBuffer+MFRC522::MF_KEY_SIZE, Settings::AccessBits, Settings::accessBitsCount);

        // Set KeyB.
        memcpy(keyBuffer+MFRC522::MF_KEY_SIZE+4, m_PiccKeyB.keyByte, MFRC522::MF_KEY_SIZE);

        // Subtracting one from the first data block value get the sector trailer
        // block address. Static cast is used because of the narrowing conversion
        // caused by using an LValue in the subtraction.
        byte sectorTrailer {static_cast<byte>(m_blockAuth.block0Addr + Settings::sectorBlocks - 1)};

        // Serial.print(F("Sector block used :"));
        // Serial.println(sectorTrailer);

        // Serial.println(F("Sector Block Configuration :"));
        // dumpBytes(keyBuffer, Settings::blockSize);
        // return;

        // authenticate the sector trailer block before attempting a write operation.
        // Consecutive change of the same sector trailer will require KeyB as the
        // modified access bits block KeyA from every accessing the sector trailer block
        // anymore.
        status = m_rc522.PCD_Authenticate(
            MFRC522::PICC_CMD_MF_AUTH_KEY_A,    // authenticate with Key A
            sectorTrailer,                      // block number
            &m_blockAuth.authKeyA,              // Key B is same as Key A for a new tag.
            &(m_rc522.uid)                      // Selected Card Uid
        );

        // Serial.print(F("MIFARE_Auth Sector trailer auth: "));
        // Serial.println(m_rc522.GetStatusCodeName(status));

        // On successful authentication attempt to write the sector trailer block.
        if (status == MFRC522::STATUS_OK)
            status = m_rc522.MIFARE_Write(
                sectorTrailer,
                keyBuffer, 
                Settings::blockSize
            );

        // Serial.print(F("MIFARE_Write Sector trailer reading: "));
        // Serial.println(m_rc522.GetStatusCodeName(status));

        if (status == MFRC522::STATUS_OK)
            setDetailsMsg((char*)"Upgrading key config was successful!");
        else
            setDetailsMsg((char*)"Upgrading key config failed!");
    }

    return status;
}