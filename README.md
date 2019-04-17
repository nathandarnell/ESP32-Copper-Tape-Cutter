# ESP32-Copper-Tape-Cutter

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
