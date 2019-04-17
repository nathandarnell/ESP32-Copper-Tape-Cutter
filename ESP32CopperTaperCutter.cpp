#include <Arduino.h>
#include <AccelStepper.h>
#include <ESP32Servo.h> // Needed for servos with ESP32
// Change ESP32Servo.h based on what servo you are using
#include <SPI.h>
#include <TFT_eSPI.h>
// Pin Description for this screen: 
// https://www.arthurwiz.com/software-development/177-inch-tft-lcd-display-with-st7735s-on-arduino-mega-2560
// GND - GND
// VCC - 3.3v
// SCK - TFT_SCLK 5
// SDA - TFT_MOSI 17
// RES - TFT_RST  16
// RS  - TFT_DC  4
// CS  - TFT_CS   15
// LEDA- 3.3v
#include <EEPROM32_Rotate.h>
// A nice explanation of how this library works: http://tinkerman.cat/eeprom-rotation-for-esp8266-and-esp32/
#include <menu.h>
#include <menuIO/TFT_eSPIOut.h>
#include <streamFlow.h>
#include <ClickEncoder.h>
// Using this library: https://github.com/soligen2010/encoder.git
#include <menuIO/clickEncoderIn.h>
#include <menuIO/keyIn.h>
#include <menuIO/chainStream.h>
#include <menuIO/serialIO.h> 
// For debugging the TFT_eSPIOut.h library
#include <menuIO/serialOut.h>
#include <menuIO/serialIn.h>

using namespace Menu;

// Define the stepper pins
#define STEP 26
#define DIR 27
#define ENABLE 25

// Define pin for servo control
#define SERVOPIN 14

// Declare pins for rotary encoder
#define encA 35
#define encB 32
#define encBtn 34
#define encSteps 4

// Setup TFT colors.  Probably stop using these and use the colors defined by ArduinoMenu
#define BACKCOLOR TFT_BLACK
#define TEXTCOLOR TFT_WHITE

EEPROM32_Rotate EEPROMr;

// Taken from :https://robotic-controls.com/learn/arduino/organized-eeprom-storage
// And from: https://embeddedgurus.com/stack-overflow/2009/11/keeping-your-eeprom-data-valid-through-firmware-updates/
struct storageStruct
{
    //stuff in here gets stored to the EEPROM
    uint16_t firmwareVersion;  // Firmware version to check if we need to overwrite
    int16_t servoOpen;         // Servo position in degrees
    int16_t servoClosed;       // Servo position in degrees
    float stepperMaxSpeed;     // Stepper Max Speed
    float stepperAcceleration; // Stepper Max Acceleration
    float stepsPerMM;          // Stepper steps per mm
    bool feedDir;              // Feed direction
};

struct storageStruct EEPROMSTORAGE; // Temorary struct for reading EEPROM into

struct storageStruct settingsEEPROM = {
    12,      // Firmware version to check if we need to overwrite
    105,     // Servo open position in degrees
    0,       // Servo closed position in degrees
    4000.0,  // Stepper Max Speed
    4000.0,  // Stepper Max Acceleration
    34.1259, // Stepper steps per mm
    1,       // Feed direction
}; 

const uint16_t EEPROM_storageSize = sizeof(EEPROMSTORAGE);
// const uint16_t EEPROMStartAddress = 0;


#define EEPROMStartAddress 0 // Where to start reading and writing EEPROM


int numberOfCuts = 5;
int cutsMade = 0;
int lengthOfCuts = 50; // Length in mm
int feedLength = 304;
int exitMenuOptions = 0; //Forces the menu to exit and cut the copper tape



// Instantiate the servo and stepper
Servo servo;
AccelStepper stepperMotor(AccelStepper::DRIVER, STEP, DIR, true); // (type,step,dir)

// Declare the clickencoder
// encBtn, was the third option but made a doubleclick when combined with the keyboard below
ClickEncoder clickEncoder = ClickEncoder(encA, encB, encSteps);
ClickEncoderStream encStream(clickEncoder, 1);

// A keyboard with only one key as the encoder button
// Negative pin numbers use internal pull-up, this is on when low
keyMap encBtn_map[] = {{ -encBtn, defaultNavCodes[enterCmd].ch}};
keyIn<1> encButton(encBtn_map);//1 is the number of keys

// TFT gfx is what the ArduinoMenu TFT_eSPIOut.h is expecting
TFT_eSPI gfx = TFT_eSPI();


void feedInOut();           // Feed the tape in or out for a certain length
void servoCut();            // Handles cutting including delays and distance
void runCuts();             // Main loop that feeds tape and calls servoCuts()
void IRAM_ATTR onTimer();   // Start the timer to read the clickEncoder every 1 ms
void checkEEPROM();         //
void readEEPROM();          //
void writeEEPROM();         //
void writeEEPROMDefaults(); //
void displayEEPROM();       // Serialprint the values in EEPROM


//////////////////////////////////////////////////////////
// Start ArduinoMenu
//////////////////////////////////////////////////////////

result doFeed() {
  delay(500);
  exitMenuOptions = 2;
  return proceed;
}

result doRunCuts() {
  delay(500);
  exitMenuOptions = 1;
  return proceed;
}

result updateEEPROM() {
  writeEEPROM();
  return quit;
}

class confirmEEPROM:public menu {
public:
  confirmEEPROM (constMEM menuNodeShadow& shadow):menu(shadow) {}
  Used printTo(navRoot &root,bool sel,menuOut& out, idx_t idx,idx_t len,idx_t p) override {
    return idx<0?//idx will be -1 when printing a menu title or a valid index when printing as option
      menu::printTo(root,sel,out,idx,len,p)://when printing title
      out.printRaw((constText*)F("<Save to EEPROM"),len);//when printing as regular option
  }
};

//using the customized menu class
//note that first parameter is the class name
altMENU(confirmEEPROM,subMenu,"Save to EEPROM?",doNothing,noEvent,wrapStyle,(Menu::_menuData|Menu::_canNav)
  ,OP("Yes",updateEEPROM,enterEvent)
  ,EXIT("Cancel")
);

MENU(subMenuAdjustServo, "Adjust Servo Settings", doNothing, noEvent, noStyle
    ,FIELD(settingsEEPROM.servoOpen, "Servo Open", " degrees", 0, 180, 10, 1, doNothing, noEvent, noStyle)
    ,FIELD(settingsEEPROM.servoClosed, "Servo Closed", " degrees", 0, 180, 10, 1, doNothing, noEvent, noStyle)
    ,SUBMENU(subMenu)
    ,EXIT("<Back")
  );

CHOOSE(settingsEEPROM.feedDir, feedDirChoose, "Choose Direction:", doNothing, noEvent, noStyle
    ,VALUE("Forward", 1, doNothing, noEvent)
    ,VALUE("Backwards", 0, doNothing, noEvent)
    );

MENU(subMenuAdjustStepper, "Adjust Stepper Settings", doNothing, noEvent, noStyle
    ,FIELD(settingsEEPROM.stepperMaxSpeed, "Max Speed", "", 0, 4000, 10, 1, doNothing, noEvent, noStyle)
    ,FIELD(settingsEEPROM.stepperAcceleration, "Max Acceleration", "", 0, 4000, 10, 1, doNothing, noEvent, noStyle)
    ,FIELD(settingsEEPROM.stepsPerMM, "Steps per mm", "", 0, 100, 1, 0.01, doNothing, noEvent, noStyle)
    ,SUBMENU(feedDirChoose)
    ,SUBMENU(subMenu)
    ,EXIT("<Back")
    );

MENU(subMenuFeedInOut, "Feed Tape", doNothing, noEvent, noStyle
     , FIELD(feedLength, "Length of Feed:", "mm", 0, 1000, 10, 1, doNothing, noEvent, noStyle)
     , SUBMENU(feedDirChoose)
     , OP("Run!", doFeed, enterEvent)
     , EXIT("<Back")
    );

MENU(mainMenu, "COPPER TAPE CUTTER", doNothing, noEvent, wrapStyle
     , FIELD(lengthOfCuts, "Cut Size:", "mm", 0, 2000, 10, 1, doNothing, noEvent, noStyle)
     , FIELD(numberOfCuts, "Pieces:", "", 0, 1000, 10, 1, doNothing, noEvent, noStyle)
     , OP("Cut!", doRunCuts, enterEvent)
     , SUBMENU(subMenuFeedInOut)
     , SUBMENU(subMenuAdjustStepper)
     , SUBMENU(subMenuAdjustServo)
    );

#define MAX_DEPTH 3

// define menu colors --------------------------------------------------------
#define Black RGB565(0,0,0)
#define Red RGB565(255,0,0)
#define Green RGB565(0,255,0)
#define Blue RGB565(0,0,255)
#define Gray RGB565(128,128,128)
#define LighterRed RGB565(255,150,150)
#define LighterGreen RGB565(150,255,150)
#define LighterBlue RGB565(150,150,255)
#define LighterGray RGB565(211,211,211)
#define DarkerRed RGB565(150,0,0)
#define DarkerGreen RGB565(0,150,0)
#define DarkerBlue RGB565(0,0,150)
#define Cyan RGB565(0,255,255)
#define Magenta RGB565(255,0,255)
#define Yellow RGB565(255,255,0)
#define White RGB565(255,255,255)
#define DarkerOrange RGB565(255,140,0)

// TFT color table
const colorDef<uint16_t> colors[] MEMMODE = {
  //{{disabled normal,disabled selected},{enabled normal,enabled selected, enabled editing}}
  {{(uint16_t)Black, (uint16_t)Black}, {(uint16_t)Black, (uint16_t)Red,   (uint16_t)Red}}, //bgColor
  {{(uint16_t)White, (uint16_t)White},  {(uint16_t)White, (uint16_t)White, (uint16_t)White}},//fgColor
  {{(uint16_t)Red, (uint16_t)Red}, {(uint16_t)Yellow, (uint16_t)Yellow, (uint16_t)Yellow}}, //valColor
  {{(uint16_t)White, (uint16_t)White}, {(uint16_t)White, (uint16_t)White, (uint16_t)White}}, //unitColor
  {{(uint16_t)White, (uint16_t)Gray},  {(uint16_t)Black, (uint16_t)Red,  (uint16_t)White}}, //cursorColor
  {{(uint16_t)White, (uint16_t)Yellow}, {(uint16_t)Black,  (uint16_t)Red,   (uint16_t)Red}}, //titleColor
};


// Define the width and height of the TFT and how much of it to take up
#define GFX_WIDTH 160
#define GFX_HEIGHT 128
#define fontW 6
#define fontH 9

constMEM panel panels[] MEMMODE = {{0, 0, GFX_WIDTH / fontW, GFX_HEIGHT / fontH}}; // Main menu panel
//constMEM panel panelsOverlay[] MEMMODE = {{2, 2, GFX_WIDTH / fontW * 0.8, GFX_HEIGHT / fontH * 0.8}}; // Overlay panel
navNode* nodes[sizeof(panels) / sizeof(panel)]; //navNodes to store navigation status
panelsList pList(panels, nodes, sizeof(panels) / sizeof(panel)); //a list of panels and nodes
//idx_t tops[MAX_DEPTH]={0,0}; // store cursor positions for each level
idx_t eSpiTops[MAX_DEPTH] = {0};
TFT_eSPIOut eSpiOut(gfx, colors, eSpiTops, pList, fontW, fontH + 1);
//SERIAL_OUT(Serial);
idx_t serialTops[MAX_DEPTH] = {0};
serialOut outSerial(Serial, serialTops);
menuOut* constMEM outputs[] MEMMODE = {&eSpiOut, &outSerial}; //list of output devices

outputsList out(outputs, sizeof(outputs) / sizeof(menuOut*)); //outputs list

serialIn serial(Serial);

MENU_INPUTS(in, &encStream, &encButton, &serial);

NAVROOT(nav, mainMenu, MAX_DEPTH, in, out);

// ESP32 timer thanks to: http://www.iotsharing.com/2017/06/how-to-use-interrupt-timer-in-arduino-esp32.html
// and: https://techtutorialsx.com/2017/10/07/esp32-arduino-timer-interrupts/
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

//////////////////////////////////////////////////////////
// End Arduino Menu
//////////////////////////////////////////////////////////


void setup()
{
  Serial.begin(115200);
  delay(3000);
  
  pinMode(ENABLE, OUTPUT);
  digitalWrite(ENABLE, LOW);

  // Init EEPROM32_Rotate ----------------------------------------------------

  // You can add partitions manually by name
  EEPROMr.add_by_name("eeprom");
  EEPROMr.add_by_name("eeprom2");

  // Or add them by subtype (it will search and add all partitions with that subtype)
  //EEPROMr.add_by_subtype(0x99);

  // Offset where the magic bytes will be stored (last 16 bytes block)
  EEPROMr.offset(0xFF0);

  // Look for the most recent valid data and populate the memory buffer
  EEPROMr.begin(4096);

  // commit 512 bytes of ESP8266 flash (for "EEPROM" emulation)
  // this step actually loads the content (512 bytes) of flash into
  // a 512-byte-array cache in RAM
  Serial.print("EEPROM size is: ");
  Serial.println(EEPROM_storageSize);

  Serial.println("Reading EEPROM");  
  readEEPROM();

  Serial.print("EEPROMSTORAGE.servoOpen is: ");
  Serial.println(EEPROMSTORAGE.servoOpen);
  Serial.print("settingsEEPROM.servoOpen is: ");
  Serial.println(settingsEEPROM.servoOpen);

  Serial.println("Checking EEPROM");
  checkEEPROM();
  Serial.print("EEPROMSTORAGE.servoOpen is: ");
  Serial.println(EEPROMSTORAGE.servoOpen);
  Serial.print("settingsEEPROM.servoOpen is: ");
  Serial.println(settingsEEPROM.servoOpen);

  servo.attach(SERVOPIN);
  servo.write(settingsEEPROM.servoOpen);      // Turn SG90 servo to open degrees


  stepperMotor.setMaxSpeed(settingsEEPROM.stepperMaxSpeed);
  stepperMotor.setAcceleration(settingsEEPROM.stepperAcceleration);


  // // ESP32 timer
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1000, true);
  timerAlarmEnable(timer);


  // Use this initializer if you're using a 1.8" TFT
  SPI.begin();
  gfx.init(); // Initialize a ST7735S chip
  gfx.setRotation(3);
  //  Serial.println("Initialized ST7735S TFT");
  gfx.fillScreen(TFT_BLACK);
  //  Serial.println("done");
  delay(1000);

  pinMode(encBtn, INPUT); // Was: INPUT_PULLUP but already has pullup resistor on it

  nav.showTitle = true; // SHow titles in the menus and submenus
  //  nav.timeOut = 60;  // Timeout after 60 seconds of inactivity and return to the sensor read screen
  //  nav.idleOn(); // Start with the main screen and not the menu

}

void loop()
{
  // Slow down the menu redraw rate
  constexpr int menuFPS = 1000 / 30;
  static unsigned long lastMenuFrame = - menuFPS;
  unsigned long now = millis();
  //... other stuff on loop, will keep executing
  switch (exitMenuOptions) {
    case 1: {
        delay(500); // Pause to allow the button to come up
        runCuts(); 
        break;
      }
    case 2: {
        delay(500); // Pause to allow the button to come up
        feedInOut();
        break;
      }
    default: // Do the normal program functions with ArduinoMenu
      if (now - lastMenuFrame >= menuFPS) {
        lastMenuFrame = millis();
        nav.poll(); // Poll the input devices
      }
  }

}

void feedInOut() {
  gfx.fillScreen(TFT_BLACK); // Set the screen to black and overwrite the menu system
  gfx.setTextColor(TEXTCOLOR, BACKCOLOR); // Set the text color
  gfx.drawRect(0, 0, 160, 127 , TFT_RED); // Draw the outline of the screen in red

  gfx.setTextSize(1); // Set the size to normal again
  gfx.drawRect(0, 0, 160, 127, TFT_RED); // Draw the outline of the screen in red 
  gfx.drawString("Feeding: ", 10, 10);
  gfx.setTextSize(3); // Set the size to big
  gfx.drawNumber(feedLength, 85, 25);
  gfx.setTextSize(1); // Set the size to normal again
  gfx.drawString("Feed Direction:", 10, 73);

  stepperMotor.setCurrentPosition(0); // Reset the stepper to 0 just in case
  if (settingsEEPROM.feedDir == 1) {
    gfx.setTextSize(3); // Set the size to big
    gfx.drawString("Forward", 5, 88);
    stepperMotor.runToNewPosition((settingsEEPROM.stepsPerMM * feedLength));
  }
  if (settingsEEPROM.feedDir == 0) {
    gfx.setTextSize(3); // Set the size to big
    gfx.drawString("Backward", 5, 88);
    stepperMotor.runToNewPosition(-(settingsEEPROM.stepsPerMM * feedLength));
  }

  if (stepperMotor.distanceToGo() == 0) {
    gfx.fillScreen(TFT_BLACK); // Set the screen to black
    gfx.setTextSize(1); // Set the size to normal again
    exitMenuOptions = 0; // Return to the menu
    subMenuFeedInOut.dirty = true;
  }
}

void servoCut() {
  servo.write(settingsEEPROM.servoClosed);      // Turn SG90 servo to closed degrees
  delay(500);
  servo.write(settingsEEPROM.servoOpen);      // Turn SG90 servo to open degrees
  delay(300);
}


void runCuts() {
  gfx.fillScreen(TFT_BLACK); // Set the screen to black and overwrite the menu system
  gfx.setTextColor(TEXTCOLOR, BACKCOLOR); // Set the text color
  gfx.drawRect(0, 0, 160, 127 , TFT_RED); // Draw the outline of the screen in red
  gfx.drawFastHLine(0, 60, 160, TFT_RED); // Draw the line between cuts made and left in red 
  stepperMotor.setCurrentPosition(0); // Reset the stepper to 0 just in case

  for (int i = 0; i < numberOfCuts; i++) {
    gfx.fillScreen(TFT_BLACK); // Set the screen to black and overwrite the last number
    gfx.setTextSize(1); // Set the size to normal again
    gfx.drawRect(0, 0, 160, 127, TFT_RED); // Draw the outline of the screen in red 
    gfx.drawFastHLine(0, 64, 160, TFT_RED); // Draw the line between cuts made and left in red 
    gfx.drawString("Cuts Remaining:", 10, 10);
    gfx.setTextSize(4); // Set the size to big
    int16_t cutsLeft = (numberOfCuts - i);
    gfx.drawNumber(cutsLeft, 85, 25);

    gfx.setTextSize(1); // Set the size to normal again
    gfx.drawString("Cuts Made:", 10, 73);
    gfx.setTextSize(4); // Set the size to big
    gfx.drawNumber(i, 85, 88);

    // Run it backwards
    if (settingsEEPROM.feedDir == 0) {
      stepperMotor.runToNewPosition(-(settingsEEPROM.stepsPerMM * lengthOfCuts));
    }
    // Run it forwards
    else {
      stepperMotor.runToNewPosition((settingsEEPROM.stepsPerMM * lengthOfCuts));
    }
    if (stepperMotor.distanceToGo() == 0) {
      // Serial.println(stepperMotor.currentPosition());
      delay(200); // Wait before cutting
      servoCut(); // Cut the tape
      cutsMade += 1; // Increase the counter
      stepperMotor.setCurrentPosition(0); // Reset the position to 0 for the enxt cut
    }
  }
  
  gfx.fillScreen(TFT_BLACK); // Set the screen to black
  gfx.setTextSize(1); // Set the size to normal again
  exitMenuOptions = 0; // Return to the menu
  mainMenu.dirty = true; // Force the main menu to redraw itself
}

// ESP32 timer
void IRAM_ATTR onTimer() {
  clickEncoder.service();
}


// Read EEPROM into the EEPROMSTORAGE struct
// From a post of reading/writing structs to EEPROM: http://forum.arduino.cc/index.php?topic=479904.0
void readEEPROM() {
    EEPROMr.get(EEPROMStartAddress, EEPROMSTORAGE); 
}


// Check EEPROM for the version and overwrite with defaults if they don't match
void checkEEPROM() {
  if (EEPROMSTORAGE.firmwareVersion != settingsEEPROM.firmwareVersion) {
    // Overwrite with defaults
    Serial.println("Firmware value does not match so writing defaults!");
    Serial.println(EEPROMSTORAGE.firmwareVersion);
    Serial.println(settingsEEPROM.firmwareVersion);
    writeEEPROMDefaults();
    displayEEPROM();
  }
  else {
    // Do not overwrite leave values alone
    Serial.println("Firmware values are lining up so reading EEPROM values into settings");
    settingsEEPROM = EEPROMSTORAGE;
    displayEEPROM();
  }
}

// From a post of reading/writing structs to EEPROM: http://forum.arduino.cc/index.php?topic=479904.0
void writeEEPROM() {
    // From: https://docs.particle.io/reference/device-os/firmware/electron/#put-
    Serial.println("Pausing timer");
    if (timer != NULL) // From: https://github.com/bdring/TWANG32/blob/master/TWANG32/settings.h
		  timerStop(timer);	
    Serial.println("Writing current settings to EEPROM");
    EEPROMr.put(EEPROMStartAddress, settingsEEPROM); // Store the object to EEPROM
    Serial.println("Committing to EEPROM");
    EEPROMr.commit();
    readEEPROM();
    Serial.println("Restarting timer");
    if (timer != NULL)
      timerRestart(timer);
    displayEEPROM();
}

void writeEEPROMDefaults() {
  // From: https://docs.particle.io/reference/device-os/firmware/electron/#put-
    Serial.println("Writing default values to EEPROM");
    EEPROMr.put(EEPROMStartAddress, settingsEEPROM); // Store the object to EEPROM
    EEPROMr.commit();
    Serial.println("Committing to EEPROM");
    readEEPROM();
    
}

void displayEEPROM() {
  Serial.println("Values in EEPROMSTORAGE");
  Serial.println(EEPROMSTORAGE.firmwareVersion);
  Serial.println(EEPROMSTORAGE.servoOpen);
  Serial.println(EEPROMSTORAGE.servoClosed);
  Serial.println(EEPROMSTORAGE.stepperMaxSpeed);
  Serial.println(EEPROMSTORAGE.stepperAcceleration);
  Serial.println(EEPROMSTORAGE.stepsPerMM);
  Serial.println(EEPROMSTORAGE.feedDir);
  
  Serial.println("Values in settingsEEPROM");
  Serial.println(settingsEEPROM.firmwareVersion);
  Serial.println(settingsEEPROM.servoOpen);
  Serial.println(settingsEEPROM.servoClosed);
  Serial.println(settingsEEPROM.stepperMaxSpeed);
  Serial.println(settingsEEPROM.stepperAcceleration);
  Serial.println(settingsEEPROM.stepsPerMM);
  Serial.println(settingsEEPROM.feedDir);
}
