/*!
 * @file rfid-plus-display.ino
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

// handleInterrupt sets that an interrupt has been detected allowing it to be
// responded to immediately.
void handleInterrupt() {  onInterrupt = true; }

// Main function.
int main(void)
{
	init();

#if defined(USBCON)
	USBDevice.attach();
#endif

    Serial.begin(Settings::SERIAL_BAUD_RATE);
    Serial1.begin(Settings::SERIAL_BAUD_RATE);

    // Wait for at least 5 secs before timing out a serial1 readbytes operation.
    Serial1.setTimeout(Settings::AUTH_DELAY);

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

    // Initiate the devices configuration during the loading state.
    rfid.setState(Transmitter::Loading);
    rfid.setDetailsMsg((char*)"WiFi and devices configuration in progress  ", false);
    
    // Delay further initialization progress until the WiFi is configured.
    Serial1.write(Settings::ACK_SIGNAL); // Send ACK signal.

    char buffer[Settings::READY_SIGNAL_SIZE];

    // Waits until the Ready signal is received. Serial1.readBytes() takes
    // about 5 secs to timeout thus no delay function is neccesary here.
    for(;;)
    {
        Serial1.readBytes(buffer, Settings::READY_SIGNAL_SIZE);
        if (memcmp(Settings::READY_SIGNAL, buffer, Settings::READY_SIGNAL_SIZE) == 0)
            break; // exit the loop

        rfid.printScreen(); // handle display updates
    }

    free(buffer); // Memory not required anymore thus can be released for further use.

    // setup the IRQ pin
    pinMode(RFID_IRQ, INPUT_PULLUP);

    // Setup the RFID interrupt handler on the CHANGE mode.
    attachInterrupt(digitalPinToInterrupt(RFID_IRQ), handleInterrupt, CHANGE);

    // Clear initial false positive interrupt(s).
    do { ; } while(!onInterrupt);

    rfid.enableInterrupts(); // enable interrupts on IRQ pin.
    onInterrupt = false;

    delay(Settings::REFRESH_DELAY); // Prepare to move state machine to standby state.

    // Configuration has been successful thus state can be moved to the Standby.
    rfid.setState(Transmitter::StandBy);

	for(;;) {
        // Print to the display
        rfid.printScreen();

        // Handle the interrupt if it has been detected.
        if (onInterrupt) rfid.handleDetectedCard();

        // RFID module is requested to transmit it data so that the microcontroller
        // can confirm if there is an interrupt to be handled.
        rfid.activateTransmission();

        delay(Settings::REFRESH_DELAY);
	}

	return 0;
}
