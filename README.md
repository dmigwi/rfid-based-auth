<h1 align="center"> RFID-based Authentication PCD Controller </h1>
<p align="center">
    <img alt="pcd-device" src="https://github.com/user-attachments/assets/7b4c11a8-7f24-4856-8e64-fe00bc4b178d" height="300">
  </a>
</p>
<p align="center"><i>PCD(Proximity Coupling Device) executing firmware written in C++/Arduino.</i></p>

## - Introduction

The device shown above is comprised of a MCU (arduino), display (LCD1602), RFID reader (RC522) and Wi-Fi (ESP-01s) device inteconnected via a custom adapter. 

The code is divided into two sub-projected that are executed by the two MCU units i.e. Arduino and ESP-01s. [rfid-plus-display](/rfid-plus-display/rfid-plus-display.ino) is executed by the Arduino MCU while [wifi-module](/wifi-module/wifi-module.ino) is executed by the ESP-01s MCU.

## - Build and Upload ESP-01s MCU code

Using makefile execute the following command from the project
```
$ make compile.esp && make upload.esp
==> Setting the working directory as ./wifi-module

==> Copying the shared settings header file into ./wifi-module

==> Compiling the code on in ./wifi-module
...
==> Deleting the shared settings header file in ./wifi-module

==> Setting the working directory as ./wifi-module

==> Uploading the code on to Arduino on port /dev/cu.usbmodem14101

arduino-cli upload -p /dev/cu.usbmodem14101 --config-file ./wifi-module/arduino-cli.yaml ./wifi-module
esptool.py v3.0
Serial port /dev/cu.usbmodem14101
Connecting....
Chip is ESP8266EX
Features: WiFi
Crystal is 26MHz
MAC: ac:0b:fb:f6:d1:74
Uploading stub...
Running stub...
Stub running...
Configuring flash size...
Auto-detected Flash size: 1MB
Compressed 335920 bytes to 241712...
Writing..........................................................................................................................................................
Wrote 335920 bytes (241712 compressed) at 0x00000000 in 25.0 seconds (effective 107.4 kbit/s)...
Hash of data verified.

Leaving...
Hard resetting via RTS pin...
New upload port: /dev/cu.usbmodem14101 (serial)
```

## - Build and Upload Arduino MCU code

```
✗ make compile.rfid && make upload.rfid
==> Setting the working directory WORKING_DIR=./rfid-plus-display

==> Copying the shared settings header file into ./rfid-plus-display

==> Compiling the code on in ./rfid-plus-display

arduino-cli compile --config-file ./rfid-plus-display/arduino-cli.yaml ./rfid-plus-display
Sketch uses 14124 bytes (49%) of program storage space. Maximum is 28672 bytes.
Global variables use 1246 bytes (48%) of dynamic memory, leaving 1314 bytes for local variables. Maximum is 2560 bytes.

...
==> Deleting the shared settings header file in ./rfid-plus-display

==> Setting the working directory WORKING_DIR=./rfid-plus-display

==> Uploading the code on to Arduino on port /dev/cu.usbmodem14201

arduino-cli upload -p /dev/cu.usbmodem14201 --config-file ./rfid-plus-display/arduino-cli.yaml ./rfid-plus-display
Connecting to programmer: .
Found programmer: Id = "CATERIN"; type = S
    Software Version = 1.0; No Hardware Version given.
Programmer supports auto addr increment.
Programmer supports buffered memory access with buffersize=128 bytes.

Programmer supports the following devices:
    Device code: 0x44

New upload port: /dev/cu.usbmodem14201 (serial)
```

### ----Enjoy tinkering with the code!----