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

// // ****Function used for debug purposes Only!****
// // dumpBytes dumps the buts on the serial output.
// void dumpBytes(byte* data, byte count)
// {
//      for (byte i = 0; i < count; i++) {
//             if (data[i] < 0x10)
//                 Serial.print("0"); // leading zero added when needed.
//             Serial.print(data[i], HEX);
//             //Serial.print(F(" "));
//         }
//         Serial.println();
// }

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

    delay(100);

    // Display the bootup welcome message.
    setStatusMsg(BootUp, false);
    setDetailsMsg((char*)"The weather today is too cold for me (:!  ", true);
}

// stateToStatus returns the string version of each state translated to
// its corresponding status message. A consistent length of 16 characters
// plus a null terminator is returned for each message.
char* Display::stateToStatus(MachineState& state) const
{
    switch (state)
    {
        case BootUp:
            return (char*)"Hello, Warszawa!"; // Welcome Message. (16 chars + \0)
        case Loading:
            return (char*)"Please Wait...  "; // Welcome Message. (16 chars + \0)
        case StandBy:
            return (char*)"Scan a Tag...   "; // Waiting for a tag. (16 chars + \0)
        case ReadTag:
            return (char*)"Tag Reading...  "; // Reading the tag. (16 chars + \0)
        case Network:
            return (char*)"WiFi Commun...  "; // Network Connection. (16 chars + \0)
        case WriteTag:
            return (char*)"Tag Writing...  "; // Writing the tag. (16 chars + \0)
        default:
            return (char*)"  --Unknown!--  "; // (16 chars + \0)
    }
}

// setStatusMsg set the status message that is to be displayed on Row 1.
// This message is mostly concise with clear message and doesn't require
// scrolling. The displayNow option defaults to false unless specified
// as true.
void Display::setStatusMsg(MachineState state, bool displayNow = false)
{
    // If content mismatch, clean up the screen before printing the new content.
    strcpy(m_statusMsg.text, stateToStatus(state));
    m_statusMsg.index = 0;
    m_detailsMsg.index = 0;

    // Screen clearing is required mainly on the Standby status update
    if (state == StandBy)
    {
        // m_lcd.clear() takes upto 2 sec to clear the screen which makes it
        // look like the screen froze. Since only the second row requires
        // clearing, printing 16 whitespace chars is far much better.
        m_lcd.setCursor(0,1);
        m_lcd.print("               "); // 16 whitespace chars.
    }

    // Triggers immediate screen update if true.
    if (displayNow)
        printScreen();
}

// setDetailsMsg sets the details message that is to be displayed on Row 2.
// This message is usually a longer explanation of the status message and
// may be scrollable. The displayNow option defaults to true unless specified
// as false.
void Display::setDetailsMsg(char* data, bool displayNow = true)
{
    // If content mismatch, clean up the screen before printing the new content.
    strcpy(m_detailsMsg.text, data); // new text used by display
    m_detailsMsg.index = 0;

    // Triggers immediate screen update if true.
    if (displayNow)
        printScreen();
}

// printScreen refreshes the display so that messages longer than max characters
// supported can be scrolled from right to left.
void Display::printScreen()
{
    if (strlen(m_statusMsg.text) > 1) // Has more than just  null terminator.
        print(m_statusMsg, 0, 0); // print on Row 1

    if (strlen(m_detailsMsg.text) > 1) // Has more than just null terminator.
        print(m_detailsMsg, 0, 1); // print on Row 2
}

// print outputs the content on the display. It also manages scrolling
// if the character count is greater than the LCD can display at a go.
void Display::print(Msg &msg, byte col, byte line)
{
    int allTextLen { static_cast<int>(strlen(msg.text)) };
    int viewTextlen { allTextLen };

    // Empty view text created with null terminator accounted for.
    char viewTxt[maxColumns+1] = {'\0'};
    char *startChar {msg.text};

    // Only For texts which needs scrolling to view it all.
    if (allTextLen > maxColumns+1)
    {
        // Only move col value if cursor position is is below maxColumns
        col = (msg.index > maxColumns) ? col : (maxColumns - msg.index);
        viewTextlen = min(msg.index+1, maxColumns);
        startChar = (msg.index < maxColumns) ? msg.text : msg.text+msg.index+1-maxColumns;
    }

    // Copying the view text substring is done because cursor shifting on
    // long texts overlays some characters on display row 1. Not acceptable!
    memcpy(viewTxt, startChar, viewTextlen);

    // set the cursor to column and line arguments provided.
    m_lcd.setCursor(col, line);
    // Print a message to the LCD. Move the cursor using pointer maths
    m_lcd.print(viewTxt);

    // Increments the cursor position or resets its to zero.
    // The reseting condition is:
    //          Cursor_Position > (allTextLen + (maxColumns -1))
    if (allTextLen > maxColumns && (++msg.index > (allTextLen+maxColumns-1)))
        msg.index = 0;
}

///////////////////////////////////////////////////
// Transmitter Class Members
//////////////////////////////////////////////////

// Transmitter constructor.
Transmitter::Transmitter( byte RFID_SS, byte RFID_RST, // RFID control pins
    Display& view
)
    : Display(view), m_rc522 {RFID_SS, RFID_RST}
{
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
void Transmitter::readPICC()
{
    m_cardData.status = MFRC522::STATUS_ERROR; // set default status to error.
    m_blockAuth.status = MFRC522::STATUS_ERROR; // set default status to error.

    // Stage 1: Activate the Card (Request + Anticollision + Select)
    //  - Card is already activated and UID retrieved. Card is ready for next operations.

    // A Tag has been detected thus the machines state is updated to read tag state.
    setStatusMsg(ReadTag);
    setDetailsMsg((char*)"Init authentication to validate key!  ");

    // Stage 2: Attempt Authentication using KeyA and read block 2 address contents
    // if successful.

    // Initiate authentication first using the default main KeyA
    attemptBlock2Auth(m_blockAuth, Settings::KeyA);

    #ifdef IS_TRUST_ORG
    // Should only be run during the Trust Organization operating Mode!.
    if (m_blockAuth.status != MFRC522::STATUS_OK)
    {
        // The card has not been reprogrammed before with its UID based key.
        for (byte i {0}; i < Settings::keysCount; ++i)
        {
            attemptBlock2Auth(m_blockAuth, Settings::defaultPICCKeyAs[i]);
            if (m_blockAuth.status == MFRC522::STATUS_OK)
                break; // A key that works has been identified, break the loop.
        }
    }
    #endif

    if (m_blockAuth.status != MFRC522::STATUS_OK)
    {
        setDetailsMsg((char*)"KeyA validity failed. Try another tag!  ");
        return;
    }

    byte deviceIdBuff[sizeof(Settings::DEVICE_ID)] = {0, 0, 0, 0, 0, 0, 0, 0};

    #ifdef IS_TRUST_ORG
    // Set the actual device ID only during the trust org mode.
    // During the trust org mode, a new secret key is returned if none existed.
    memcpy(deviceIdBuff, Settings::DEVICE_ID, sizeof(Settings::DEVICE_ID));
    #endif

    // This order of packaging should never be altered!
    const byte bytesToSend {Settings::SecretKeyAuthDataSize};
    byte dataSent[bytesToSend];
    dataSent[0] = m_rc522.uid.size;                                     // copy card uid size
    memcpy(dataSent+1, m_rc522.uid.uidByte, 10);                        // copy card uid.
    memcpy(dataSent+11, deviceIdBuff, sizeof(Settings::DEVICE_ID));     // copy the current PCD ID
    memcpy(dataSent+19, m_blockAuth.block2Data, Settings::blockSize);   // copy block 2 data

    // Serial.println(F(" SecretKey Auth contents! "));
    // Serial.println(Settings::SecretKeyAuthDataSize);
    // dumpBytes(dataSent, Settings::SecretKeyAuthDataSize);

    // Stage 3: Send the block 2 Contents to the trust organization for validation.
    // - Use Serial transmission to send the data to and from the WIFI module.
    {
        // Before writing into Serial1 cleanup its buffer first.
        while(Serial1.available() > 0)
            Serial1.read(); // reads till the buffer is empty.

        // Send block 2 address data and size.
        Serial1.write(dataSent, bytesToSend);
    }

    // Request the secret key sent from the trust organization.
    byte secretKey[MFRC522::MF_KEY_SIZE] = {0, 0, 0, 0, 0, 0};

    // Handle narrowing conversion
    byte bytesRead { static_cast<byte>(Serial1.readBytes(secretKey, MFRC522::MF_KEY_SIZE))};
    if (bytesRead != MFRC522::MF_KEY_SIZE)
    {
        setDetailsMsg((char*)"Fetching the Secret Key failed. Try another tag!  ");
        return;
    }

    // Serial.println(F(" Returned SecretKey contents! "));
    // dumpBytes(secretKey, MFRC522::MF_KEY_SIZE);

    // KeyB needs to be computed using the successfully read secret key.
    // KeyA only has read-only permissions to block 2 address while KeyB has both
    // read and write permissions to the whole sector.
    setPICCAuthKeyB(secretKey);

    // Stage 4: Read the Card block contents.
    // - The data to be read is supposed to of size TrustKeySize.
    byte blocksToRead {Settings::TrustKeySize/Settings::blockSize};
    byte lastValidBlock {(byte)(m_blockAuth.block0Addr + blocksToRead)};

    if (lastValidBlock <= Settings::maxBlockNo)
        setDetailsMsg((char*)"Initiating data extraction from the tag!  ");
    else
    {
        setDetailsMsg((char*)"Tag blocks are full. Try another tag!  ");
        return;
    }

    // Reads 16 bytes (+ 2 bytes CRC_A) from the active PICC.
    byte buffer[Settings::blockSize + 2];
    byte byteCount = sizeof(buffer);

    byte startBlock {0};
    byte addr {m_blockAuth.block0Addr};

    for (;startBlock < blocksToRead && addr < Settings::maxBlockNo; ++addr)
    {
        if ((addr + 1) % Settings::sectorBlocks == 0)
            continue;   // Ignore access bit configuration block.

        // authenticate each block before attempting a read operation.
        // If the card is new, use either KeyA or KeyB as they are similar otherwise use Tag Specific KeyB.
        m_cardData.status = m_rc522.PCD_Authenticate(
            MFRC522::PICC_CMD_MF_AUTH_KEY_B,                                // authenticate with Key B
            addr,                                                           // data block number
            (m_blockAuth.isCardNew ? &m_blockAuth.authKeyA : &m_PiccKeyB),  // KeyB already preset
            &(m_rc522.uid)                                                  // Selected Card Uid
        );

        if (m_cardData.status != MFRC522::STATUS_OK)
            break; // break on block authentication failure

        m_cardData.status = m_rc522.MIFARE_Read(addr, buffer, &byteCount);
        if (m_cardData.status != MFRC522::STATUS_OK)
            break; // break on read authentication failure

        byteCount = sizeof(buffer); // reset the buffer count.

        // copy the read data without the 2 bytes of CRC_A.
        memcpy(m_cardData.readData+(startBlock* Settings::blockSize), buffer, Settings::blockSize);
        ++startBlock; // Only increment if a data block is read.
    }

    if (m_cardData.status == MFRC522::STATUS_OK)
        setDetailsMsg((char*)"Tag reading was successful!  ");
    else
        setDetailsMsg((char*)"Reading the tag failed. Try another tag!  ");
}

// networkConn establishes Connection to the wifi Module via a serial communication.
// The WIFI module then connects to the validation server where the PICC
// card data is validated.
void Transmitter::networkConn()
{
    m_cardData.status = MFRC522::STATUS_ERROR; // set default status to error.

    // With data read from the tag, connection to the validation server is established.
    setStatusMsg(Network);
    setDetailsMsg((char*)"Initiating network connection!  ");

    // In total 68 bytes should be transmitted via the serial communication.
    const int sizeOfDeviceID {sizeof(Settings::DEVICE_ID)};

    // This order of packaging should never be altered!
    byte txData[Settings::TrustKeyAuthDataSize];
    txData[0] = m_rc522.uid.size;                                       // copy card uid size.
    memcpy(txData+1, m_rc522.uid.uidByte, 10);                          // copy card uid.
    memcpy(txData+11, Settings::DEVICE_ID, sizeOfDeviceID);             // copy the current PCD ID
    memcpy(txData+19, m_cardData.readData, Settings::TrustKeySize);     // copy Trust Key data.

    // Serial.println(F(" TrustKey validation contents! "));
    // Serial.println(Settings::TrustKeyAuthDataSize);
    // dumpBytes(txData, Settings::TrustKeyAuthDataSize);

    // Make the serial transfer of the complete data.
    {
        // Before writing into Serial1 cleanup its buffer first.
        while(Serial1.available() > 0)
            Serial1.read(); // reads till the buffer is empty.

        Serial1.write(txData, Settings::TrustKeyAuthDataSize); // Write the data into the serial transmission.
    }

    // Delay to cover the network latency between the server and WiFi module.
    delay(Settings::REFRESH_DELAY);

    // read the bytes sent back from the WIFI module.
    size_t bytesRead {Serial1.readBytes(txData, Settings::TrustKeySize)};

    // Serial.println(F(" TrustKey returned contents! "));
    // Serial.println(bytesRead);
    // dumpBytes(txData, bytesRead);

    // For a successful Network Data read:
    // 1. Bytes read must match the match the size of a trust key.
    // 2. Device ID uid returned must match the existing one.
    if (bytesRead == Settings::TrustKeySize)
    {
        byte returnedDeviceID[sizeOfDeviceID];
        memcpy(returnedDeviceID, txData+40, sizeOfDeviceID); // copy device uid.

        // Serial.println(F(" Copied Device ID contents! "));
        // dumpBytes(returnedDeviceID, sizeOfDeviceID);

        if (memcmp(returnedDeviceID, Settings::DEVICE_ID, sizeOfDeviceID) == 0)
        {
            setDetailsMsg((char*)"Network connection was successful!  ");
            m_cardData.status = MFRC522::STATUS_OK; // update status
            memcpy(m_cardData.readData, txData, Settings::TrustKeySize); // update the new card data
            return;
        }
    }
    setDetailsMsg((char*)"Network connectivity failed!  ");
}

// writePICC writes the provided content to the PICC.
void Transmitter::writePICC()
{
    // With data returned from the validation server, PICC can be written.
    setStatusMsg(WriteTag);
    setDetailsMsg((char*)"Initiating tag writing operation!  ");

    byte blocksToRead {Settings::TrustKeySize/Settings::blockSize};
    byte buffer[Settings::blockSize];

    byte startBlock {0};
    byte addr {m_blockAuth.block0Addr};

    // Serial.println(F(" TrustKey contents writing! "));
    // dumpBytes(m_cardData.readData, Settings::TrustKeySize);

    for (;startBlock < blocksToRead && addr < Settings::maxBlockNo; ++addr)
    {
        if ((addr + 1) % Settings::sectorBlocks == 0)
            continue; // Ignore access bit configuration block.

        memcpy(buffer, m_cardData.readData+(startBlock* Settings::blockSize), Settings::blockSize);
        // Serial.print(F(" Block: "));
        // Serial.println(startBlock);
        // dumpBytes(buffer, Settings::blockSize);
        ++startBlock; // Only increment if a data block is read.

        m_cardData.status = m_rc522.MIFARE_Write(addr, buffer, Settings::blockSize);
        if (m_cardData.status != MFRC522::STATUS_OK)
            break;
    }

    if (m_cardData.status == MFRC522::STATUS_OK)
        setDetailsMsg((char*)"Tag writing was successful!  ");
    else
        setDetailsMsg((char*)"Writing the tag failed. Try another tag!  ");
}

// cleanUpAfterCardOps undertake reset operation back to the standby
// state after the read, network connection and write operation
// on a PICC completes.
void Transmitter::cleanUpAfterCardOps()
{
    delay(Settings::AUTH_DELAY); // Delay before making another PICC selection.

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
// is triggered which forces reading and writing of the necessary data to
// the card to be done as a matter of urgency.
void Transmitter::handleDetectedCard()
{
    if (isNewCardDetected())
    {
        readPICC();

        // Only send the cards data in the reading operation was successful.
        if (m_cardData.status == MFRC522::STATUS_OK)
            networkConn();

        // Only write the card data if the network operation was successful.
        if (m_cardData.status == MFRC522::STATUS_OK)
            writePICC();

        #ifdef IS_TRUST_ORG
        if (m_cardData.status == MFRC522::STATUS_OK)
            m_cardData.status = setUidBasedKey(); // Upgrade Key if the card is new.
        #endif

        // Handle clean up after the card operations.
        cleanUpAfterCardOps();
    }

    // Reset the machine state to standy by indicating that the device is ready
    // to handle another PICC selected.
    setStatusMsg(StandBy);
}

// setUidBasedKey replaces the non-uid base key with a Uid based which is
// quicker and safer to use. This is done on the cards detected as new.
// NB: Feature only works in the Trust Organization Mode.
MFRC522::StatusCode Transmitter::setUidBasedKey()
{
    MFRC522::StatusCode status = MFRC522::STATUS_ERROR;
    #ifdef IS_TRUST_ORG
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

        // On successful authentication attempt to write the sector trailer block.
        if (status == MFRC522::STATUS_OK)
            status = m_rc522.MIFARE_Write(
                sectorTrailer,
                keyBuffer,
                Settings::blockSize
            );

        if (status == MFRC522::STATUS_OK)
            setDetailsMsg((char*)"Upgrading key config was successful! ");
        else
            setDetailsMsg((char*)"Upgrading key config failed! ");
    }
    #endif

    return status;
}