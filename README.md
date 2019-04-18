# ESP32 Copper Tape Cutter

Every year I teach my students how to build paper circuits using templates from [Chibitronics](https://chibitronics.com/).  For the first few projects they follow a template I give them so I know exactly how much copper tape each student needs.  I made this machine to measure and cut the tape for me. 

The parts, instructions, and more are available on Thingiverse: https://www.thingiverse.com/thing:3557719

This program uses these libraries:
  * [ArduinoMenu](https://github.com/neu-rah/ArduinoMenu)
  * [streamFlow](https://github.com/neu-rah/streamFlow)
  * [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) - You will have to setup your TFT settings in the User_Setup.h file and I included my working setup for you
  * [ClickEncoder](https://github.com/soligen2010/encoder) - This is a fork of the library ArduinoMenu uses by default and both need the "#include <avr/..." lines commented out in ClickEncoder.h if you are using an ESP32
  * [AccelStepper](http://www.airspayce.com/mikem/arduino/AccelStepper/)
  * [ESP32Servo](https://github.com/jkb-git/ESP32Servo)
  * [EEPROM32 Rotate](https://github.com/xoseperez/eeprom32_rotate)

Due to how EEPROM32_Rotate creates custom partitions, I switched over from Arduino IDE to VS Code with PlatformIO.  Although the learning curve was steeper, in a few hours I felt comfortable and started to appreciate the 
Here is an overview of how to get started by [Paul O'Brian](http://paulobrien.co.nz/2018/04/02/platform-io-visual-studio-code-and-arduino-bye-bye-arduino-ide/) and the PlatformIO [Install Guide](https://platformio.org/install/ide?install=vscode) and [Quick Start](https://docs.platformio.org/en/latest/ide/vscode.html#quick-start).
