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

    // Wait for refresh delay before timing out a serial1 readbytes operation.
    Serial1.setTimeout(Settings::REFRESH_DELAY);

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

    // Initialize the display class instance first.
    Display view {
        LCD_RST, LCD_RW, LCD_EN,        // LCD Control Pins
        LCD_D4, LCD_D5, LCD_D6, LCD_D7  // LCD 4-bit UART data pins
    };

    // Give a delay to allow proper set up of the display.
    delay(Settings::AUTH_DELAY);

    // Initiate the devices configuration during the loading state.
    view.setStatusMsg(Display::Loading, false);
    view.setDetailsMsg((char*)"WiFi and devices configuration in progress  ", true);

    char buffer[Settings::READY_SIGNAL_SIZE];
    // Delay further initialization progress until the WiFi is configured.
    // Waits until the READY signal is received. Serial1.readBytes() takes
    // about 1 secs to timeout thus no delay function is neccesary here.
    for(;;)
    {
        Serial1.write(Settings::ACK_SIGNAL); // Send ACK signal.

        Serial1.readBytes(buffer, Settings::READY_SIGNAL_SIZE);
        // readByte() doesn't return a none-null terminated string thus -1 is used.
        if (memcmp(Settings::READY_SIGNAL, buffer, Settings::READY_SIGNAL_SIZE-1) == 0)
            break; // exit the loop

        view.printScreen(); // handle display updates
    }

    free(buffer); // Memory not required anymore thus can be released for further use.

    // Once WiFi and devices configuration is successful, the transmitter class
    // can now be properly initialized.
    Transmitter rfid {RFID_SS, RFID_RST, view};

    // Wait for at least 5 secs before timing out a serial1 readbytes operation.
    Serial1.setTimeout(Settings::AUTH_DELAY);

    // setup the IRQ pin
    pinMode(RFID_IRQ, INPUT_PULLUP);

    // Setup the RFID interrupt handler on the CHANGE mode.
    attachInterrupt(digitalPinToInterrupt(RFID_IRQ), handleInterrupt, CHANGE);

    // Clear initial false positive interrupt(s).
    do { ; } while(onInterrupt);

    rfid.enableInterrupts(); // enable interrupts on IRQ pin.
    onInterrupt = false;

    rfid.timerDelay(Settings::REFRESH_DELAY); // Prepare to move state machine to standby state.

    // Configuration has been successful thus state can be moved to the Standby.
    rfid.setStatusMsg(Display::StandBy, false);
    rfid.setDetailsMsg((char*)"The weather today is too cold for me (:!  ", true);

	for(;;) {
        if (onInterrupt)
            // Handle the interrupt if it has been detected.
            rfid.handleDetectedCard();
        else
            // Request RFID module transmits its data so that the microcontroller
            // can confirm if there is an interrupt to be handled.
            rfid.activateTransmission();

        // Timer delay also prints the contents to the display.
        rfid.timerDelay(Settings::REFRESH_DELAY);
	}

	return 0;
}
