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

namespace CommonRFID
{
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

    // AuthDataSize defines the size data expected when validating
    // a trust key read from the NFC tag.
    // 1 byte => UID size, either of (4/7/10)
    // 10 bytes => card's UID Data
    // 8 bytes => Current PCD's ID
    // 48 bytes => Trust Key Data
    // 1 byte => size of the rest of data size expected.
    // In total 68 bytes should be transmitted via the serial communication.
    // Since the first byte is read separately, subtract 1 to get 67 bytes.
    constexpr int AuthDataSize {67};

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