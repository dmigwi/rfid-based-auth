/*!
 * @file commonRFID.h
 *
 * @section intro_sec Introduction
 *
 * This file is part rfid-based-auth project files. It holds the common settings
 * configurations that are shared between rfid-plus-display and the wifi-module
 * sub-projects.
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

#ifndef _COMMON_RFID_CONFIG_
#define _COMMON_RFID_CONFIG_

#include "arduino.h"

// IS_TRUST_ORG flag is used to indicate that the current PCD mode allows a trust
// organization to over write blank memory space if no previous Trust Key exists.
// #define IS_TRUST_ORG

namespace CommonRFID
{
    // TODO: A more easier approach to update it should be implemented.
    #ifdef IS_TRUST_ORG
    constexpr byte DEVICE_ID[] {0xef, 0x12, 0x34, 0x56, 0xab, 0xcd, 0xef, 0xab};
    #else
    constexpr byte DEVICE_ID[] {0xef, 0x12, 0x34, 0x56, 0xab, 0xcd, 0xef, 0x12};
    #endif

    // SERVER_API_URL defines the trust organisation server API url that this
    // device supports.
    static const char* SERVER_API_URL = {"http://dmigwi.atwebpages.com/rfid-based-auth/"};

    // SERIAL_BAUD_RATE defines the data communication rate to be used during
    // serial communication.
    constexpr long int SERIAL_BAUD_RATE {115200};

    // REFRESH_DELAY defines the interval at which components are refreshed
    // e.g the display.
    constexpr int REFRESH_DELAY {700};

    // AUTH_DELAY defines the length in ms the systems waits to initiate a new
    // authentication after the previous one finished.
    constexpr int AUTH_DELAY {5000};

    // blockSize defines the size of a block data that is read from
    // an NFC tag's sector with the Trust Key.
    constexpr int blockSize {16};

    // TrustKeySize defines the total size in bytes that stores the Trust Key.
    // The data read is supposed to be 384 bits long, translating
    // to 384/8 = 48 bytes. Since each block is of 16 bytes memory capacity,
    // then 3 consecutive blocks will be adequate to store 384 bit/ 48 bytes.
    constexpr byte TrustKeySize{48};

    // SecretKeyAuthDataSize defines the size of API data sent from PCD to the backend
    // servers when authenticating block 2 data. It contains:
    // 1 byte => UID size, either of (4/7/10)
    // 10 bytes => card's UID Data
    // 8 bytes => Current PCD's ID
    // 16 bytes => Block 2 Data
    // In total 35 bytes should be transmitted via the serial communication.
    // NB: Data is packaged in the order above as from byte zero.
    constexpr byte SecretKeyAuthDataSize {35};

    // TrustKeyAuthDataSize defines the size of API data sent from PCD to the backend
    // servers when validating a trust key read from the NFC tag. It contains:
    // 1 byte => UID size, either of (4/7/10)
    // 10 bytes => card's UID Data
    // 8 bytes => Current PCD's ID
    // 48 bytes => Trust Key Data
    // In total 67 bytes should be transmitted via the serial communication.
    // NB: Data is packaged in the order above as from byte zero.
    constexpr byte TrustKeyAuthDataSize {67};

    // MaxReqSize the maximum size of the data from the serial communication
    // can be read into contagious memory location.
    constexpr int MaxReqSize {72};

    // ACK_SIGNAL_SIZE defines the number of chars in the ack signal
    // including the null terminator.
    const byte ACK_SIGNAL_SIZE {4};

    // READY_SIGNAL_SIZE defines the number of chars in the ready signal
    // including the null terminator.
    const byte READY_SIGNAL_SIZE {6};

    // ACK_SIGNAL is a signal sent serially requesting the WiFi module to
    // confirm that it is ready to process http requests.
    const char ACK_SIGNAL[ACK_SIGNAL_SIZE] {"ACK"};

    // READY_SIGNAL is the response that the WiFi module sends back once it has
    // booted up and made the proper connections to an external WiFi ACCESS POINT.
    const char READY_SIGNAL[READY_SIGNAL_SIZE] {"READY"};
};

#endif