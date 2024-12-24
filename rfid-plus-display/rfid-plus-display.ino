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

void runSerialPassthrough()
{
    #ifdef ARDUINO_AVR_LEONARDO
    // SerialPassthrough code is only run when needed.
    // https://docs.arduino.cc/built-in-examples/communication/SerialPassthrough/
    // A serial passthrough is necessary because leonardo board doesn't have
    // a chip dedicated to managing the serial communication via UART protocol.
    // Copy from virtual serial line to uart and vice versa
    if (Serial.available()) {
        while(Serial.available())
            Serial1.write(Serial.read());
    }

    if (Serial1.available()) {
        while(Serial1.available())
            Serial.write(Serial1.read());
    }
    #endif

    if (serialEventRun) serialEventRun();
}

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

    // setup the IRQ pin
    pinMode(RFID_IRQ, INPUT_PULLUP);

    // Setup the RFID interrupt handler on the CHANGE mode.
    attachInterrupt(digitalPinToInterrupt(RFID_IRQ), handleInterrupt, CHANGE);

    // Clear initial false positive interrupt(s).
    do { ; } while(!onInterrupt);

    rfid.enableInterrupts(); // enable interrupts on IRQ pin.
    onInterrupt = false;

    Serial.begin(Settings::SERIAL_BAUD_RATE);
    Serial1.begin(Settings::SERIAL_BAUD_RATE);

    delay(Settings::REFRESH_DELAY); // Prepare to move machine to standby state.

    // Machine has successfully booted up thus can be moved to the Standby State.
    rfid.setState(Transmitter::StandBy);

	for(;;) {
        // Print to the display
        rfid.printScreen();

        // Handle the interrupt if it has been detected.
        if (onInterrupt) rfid.handleDetectedCard();

        runSerialPassthrough();

        // RFID module is requested to transmit it data so that the microcontroller
        // can confirm if there is an interrupt to be handled.
        rfid.activateTransmission();

        delay(Settings::REFRESH_DELAY);
	}

	return 0;
}
