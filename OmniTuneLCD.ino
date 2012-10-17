///////////////////////////////////////////////////////////////////////////////
//
// OmniTune
//
// Radio tuner firmware for X-Plane. Designed for a 16x2 display and two
// encoders with integral pushbuttons, or similar.
//
// This code is written for the PJRC Teensy board, v2.0 or higher, using the
// Arduino+Teensyduino framework. This instance of the code is completely
// independent of the PC, other than for power; it does not connect to X-Plane.
//
// Copyright 2012 Jack Deeth
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// I would appreciate, but not insist, on attribution if this code is
// incorporated into other projects.
//
///////////////////////////////////////////////////////////////////////////////

#include <LiquidCrystalFast.h>
#include <Bounce.h>
#include <Encoder.h>

///////////////////////////////////////////////////////////////////////////////
// Hardware setup
//

///////////////////
// Output hardware
//
enum LCD_PINS {
  RS = 39,
  RW,
  EN,     // these are automatically numbered from 39 to 45
  D4,     // thanks to the magic of enum
  D5,
  D6,
  D7 // = 45
};

LiquidCrystalFast lcd(RS, RW, EN, D4, D5, D6, D7);

void setupOutput() {
  pinMode (RS, OUTPUT);
  pinMode (RW, OUTPUT);
  pinMode (EN, OUTPUT);
  pinMode (D4, OUTPUT);
  pinMode (D5, OUTPUT);
  pinMode (D6, OUTPUT);
  pinMode (D7, OUTPUT);

  lcd.begin (16, 2);

  pinMode (LED_BUILTIN, OUTPUT);
}

///////////////////
// Input hardware
//
//
// provisional plan: left up/down for coarse changes, right up/down for fine
// changes, left/right in for prev/next channel
//
enum INPUT_PINS {
  PIN_LEFT_UP = 8,
  PIN_LEFT_DOWN = 0,
  PIN_LEFT_IN = 4,

  PIN_RIGHT_UP = 17,
  PIN_RIGHT_DOWN = 12,
  PIN_RIGHT_IN = 16
};

// I am using buttons instead of encoders, which I don't have
Bounce leftUp = Bounce (PIN_LEFT_UP, 5);
Bounce leftDown = Bounce (PIN_LEFT_DOWN, 5);
Bounce leftIn = Bounce (PIN_LEFT_IN, 5);

Bounce rightUp = Bounce (PIN_RIGHT_UP, 5);
Bounce rightDown = Bounce (PIN_RIGHT_DOWN, 5);
Bounce rightIn = Bounce (PIN_RIGHT_IN, 5);

void setupInput () {
  pinMode (PIN_LEFT_UP, INPUT_PULLUP);
  pinMode (PIN_LEFT_DOWN, INPUT_PULLUP);
  pinMode (PIN_LEFT_IN, INPUT_PULLUP);
  pinMode (PIN_RIGHT_UP, INPUT_PULLUP);
  pinMode (PIN_RIGHT_DOWN, INPUT_PULLUP);
  pinMode (PIN_RIGHT_IN, INPUT_PULLUP);
}

///////////////////
// Dummy encoders
//
short leftEnc;  // these are masquerading as Encoder objects
short rightEnc;

short leftEncPrev;  // position of encoders when last inspected
short rightEncPrev;

///////////////////////////////////////////////////////////////////////////////
// X-Plane objects
// I am using ordinary integers, because I don't have a working X-Plane install

enum DATAREF_NAMES {
  NAV1, NAV2, COM1, COM2, ADF1, ADF2, XP_CODE, XP_CODE_LO, XP_MODE,
  DATAREF_COUNT
};

enum XP_MODES {
  XP_OFF, XP_STBY, XP_ON, XP_ALT, XP_TEST,
  XP_MODE_COUNT
};

int dataref[DATAREF_COUNT];
//FlightSimInteger dataref[DATAREF_COUNT];

void setupDataref() {
  // commented for offline testing
  //dataref[NAV1] = XPlaneRef("sim/cockpit2/radios/actuators/nav1_frequency_hz");
  //dataref[NAV2] = XPlaneRef("sim/cockpit2/radios/actuators/nav2_frequency_hz");
  //dataref[COM1] = XPlaneRef("sim/cockpit2/radios/actuators/com1_frequency_hz");
  //dataref[COM2] = XPlaneRef("sim/cockpit2/radios/actuators/com2_frequency_hz");
  //dataref[ADF1] = XPlaneRef("sim/cockpit2/radios/actuators/adf1_frequency_hz");
  //dataref[ADF2] = XPlaneRef("sim/cockpit2/radios/actuators/adf2_frequency_hz");
  //dataref[XP_CODE] = XPlaneRef("sim/cockpit2/radios/actuators/transponder_code");
  //dataref[XP_CODE_LO] = 0; // This should not be used
  //dataref[XP_MODE] = XPlaneRef("sim/cockpit/radios/transponder_mode");

  // dummy values for offline testing
  dataref[NAV1] = 11110;
  dataref[NAV2] = 11220;
  dataref[COM1] = 12330;
  dataref[COM2] = 12440;
  dataref[ADF1] = 345;
  dataref[ADF2] = 456;
  dataref[XP_CODE] = 1200;
  dataref[XP_CODE_LO] = 0; // should not be used
  dataref[XP_MODE] = XP_ON;
}

///////////////////////////////////////////////////////////////////////////////
// Local objects

short channel = NAV1; // indicates selected channel

//counter for flashing characters (transponder digits)
int flashCount = 0;
bool flashNow = false;

void displayUpdate();

elapsedMillis dispTimer = 0; //to avoid updating display too frequently

///////////////////////////////////////////////////////////////////////////////

void setup() {
  // by putting stuff which normally goes into setup() into these other
  // functions, we can group related stuff into the same part of this file
  setupInput();
  setupOutput();
  setupDataref();
}

void loop() {
  // updates
  FlightSim.update();

  leftUp.update();
  leftDown.update();
  leftIn.update();

  rightUp.update();
  rightDown.update();
  rightIn.update();

  //display updated every 80ms (12.5fps)
  if (dispTimer > 80) {
    dispTimer -= 80;

    displayUpdate();
    // Flashing sequence is Six On, Two Off, per display updates.
    ++flashCount %= 8; // increment flashCount between 0 and 7
    flashNow = (flashCount < 6);
    //digitalWrite(LED_BUILTIN, flashNow);

  }

  /////////////////////////////////////////////////////////////////////////////
  // Input processing

  /////////////////
  // Mode change buttons
  //
  if(leftIn.fallingEdge()) {     // when left encoder pressed
    --channel;                   // select previous channel
    while (channel < 0)          // if there's no previous channel,
      channel += DATAREF_COUNT;  // go to the last channel.
  }

  if(rightIn.fallingEdge()) {    // when right encoder pressed
    ++channel;                   // select next channel
    while (channel >= DATAREF_COUNT)
      channel -= DATAREF_COUNT;  // when we go past the last, go back to the first
  }

  /////////////////
  // Dummy encoders
  //
  if(leftUp.fallingEdge()) {
    ++leftEnc;
  }
  if(leftDown.fallingEdge()) {
    --leftEnc;
  }
  if(rightUp.fallingEdge()) {
    ++rightEnc;
  }
  if(rightDown.fallingEdge()) {
    --rightEnc;
  }
  short leftEncDiff = (leftEnc - leftEncPrev);
  short rightEncDiff = (rightEnc - rightEncPrev);
  //
  // We can substitute this code for real encoder-handling code with only
  // minimal changes to the rest of the code. We'll still have the two
  // encDiff integers to show how many detents the encoders have been turned by
  //
  /////////////////

  // reset encoders if they've been turned
  if (leftEncDiff) {
    leftEnc = 0; //substitute leftEnc.write(0) when real encoders are used
    leftEncPrev = 0;
  }

  if (rightEncDiff) {
    rightEnc = 0;
    rightEncPrev = 0;
  }

  // tune frequencies if either encoder has been turned
  if (leftEncDiff || rightEncDiff) {

    int freq = dataref[channel];

    switch (channel) { // tune selected channel

    case NAV1:
    case NAV2:
      if (leftEncDiff) {
        // TUNE HI. Increment in megaherts, which is freq * 100
        freq += leftEncDiff * 100;
        // lap to 108-118MHz range
        while (freq < 10800) freq += 1000;
        while (freq >= 11800) freq -= 1000;
      }
      if (rightEncDiff) { //TUNE LO. Increment in decaKHz
        //remove MHz element from freq, leaving decaKHz element
        int mhz = freq / 100;
        freq -= mhz * 100;
        //increment freq in 50KHz (0.05MHz) steps
        freq += rightEncDiff * 5;
        //crop freq to prevent TUNE LO mode from changing TUNE HI digits
        while (freq < 0) freq += 100;
        while (freq >= 100) freq -= 100;
        //reinstate MHz element
        freq += mhz * 100;
      }
    break;

    case COM1:
    case COM2:
      if (leftEncDiff) {//TUNE HI.
        freq += leftEncDiff * 100;
        // lap to 118.00 - 136.00
        while (freq <  11800) freq += 1800;
        while (freq >= 13600) freq -= 1800;
      }
      if (rightEncDiff) { //TUNE LO
        //remove megahertz from freq (digits left of decimal point)
        int mhz = freq / 100;
        freq -= mhz * 100;
        //COM radios change in 25KHz steps, but X-Plane crops down to 10KHz resolution (ie 120.125 becomes 12012, losing the final 5)
        //floating point variable used to reinstate missing 5 KHz when necessary
        float ffreq = freq;
        if ((freq - (10*(freq/10))) == 2 		// if dataref value ends in 2
            || (freq - (10*(freq/10))) == 7) { 	// or dataref value ends in 7
          ffreq += 0.5; 						// then reinstate missing 5KHz
        }
        //increment in 25KHz steps
        ffreq += rightEncDiff * 2.5;
        //convert back to integers (c++ drops the trailing .5 if necessary)
        freq = ffreq;
        //keep freq between 0 and <100 so this operation doesn't affect digits left of decimal point
        while (freq < 0) freq += 100;
        while (freq >= 100) freq -= 100;
        //reinstate the megahertz digits
        freq += mhz * 100;
      }
    break;

    case ADF1:
    case ADF2:
      if (leftEncDiff) { // alter first two digits
        freq += leftEncDiff * 10;
        // crop to 190-600 KHz range
        while (freq < 190) freq += 10;
        while (freq >= 600) freq -= 10;
      }
      if (rightEncDiff) { // alter third digit
        int digit12 = freq / 10;        //remove 1st and 2nd digits
        freq -= digit12 * 10;           //increment freq in 1KHz steps
        freq += rightEncDiff;
        // lap to prevent changes to 1st and 2nd digits
        while (freq < 0) freq += 10;
        while (freq >= 10) freq -= 10;
        // restore 1st and 2nd digits
        freq += digit12 * 10;
      }
    break;

    case XP_CODE_LO:
      freq = dataref[XP_CODE];
      // fall through
    case XP_CODE:
      // if transponder is active, put it into standby before altering code
      if (dataref[XP_MODE] >= XP_ON)
        dataref[XP_MODE] = XP_STBY;
      // split transponder code into digits
    {
      short codeDigit[4];
      codeDigit[0] = freq/1000;
      codeDigit[1] = (freq - 1000 *(freq/1000) ) / 100;
      codeDigit[2] = (freq - 100 * (freq/100) ) / 10;
      codeDigit[3] = freq - 10 * (freq/10);

      if (channel == XP_CODE) { // alter first two digits

        codeDigit[0] += leftEncDiff;
        // lap each digit to 0-7 range
        // (transponder is octal code)
        while (codeDigit[0] < 0) codeDigit[0] += 8;
        while (codeDigit[0] > 7) codeDigit[0] -= 8;
        codeDigit[1] += rightEncDiff;
        while (codeDigit[1] < 0) codeDigit[1] += 8;
        while (codeDigit[1] > 7) codeDigit[1] -= 8;

      } else { // alter last two digits

        codeDigit[2] += leftEncDiff;
        while (codeDigit[2] < 0) codeDigit[2] += 8;
        while (codeDigit[2] > 7) codeDigit[2] -= 8;
        codeDigit[3] += rightEncDiff;
        while (codeDigit[3] < 0) codeDigit[3] += 8;
        while (codeDigit[3] > 7) codeDigit[3] -= 8;

      } // alter codeDigit

      // recombine codeDigit
      freq = codeDigit[0] * 1000 + codeDigit[1] * 100 + codeDigit[2] * 10 + codeDigit[3];

      if(channel == XP_CODE_LO)
        dataref[XP_CODE] = freq;
    }
    break;

    case XP_MODE:
      freq += leftEncDiff;
      freq += rightEncDiff;
      if (freq < 0)
        freq = 0;
      if (freq >= XP_MODE_COUNT)
        freq = XP_MODE_COUNT - 1;
    break;

    } // switch, tune selected channel

    dataref[channel] = freq;
  }
}

///////////////////////////////////////////////////////////////////////////////
// Display Update
//
// Reads datarefs and draws selected channels onto display
//
void displayUpdate() {

  /////////////////
  // Set up display
  //
  lcd.clear();
  lcd.setCursor(0, 0);

  /////////////////
  // Print selected channels
  //
  float tmp; // for putting decimal point into integer dataref frequencies

  switch (channel) { // print selected channels
  case NAV1:
  case NAV2:
  case XP_CODE:
  case XP_CODE_LO:
  case XP_MODE:
    lcd.print("NAV1");
    if (channel == NAV1)
      lcd.print(">");
    lcd.setCursor(5, 0);
    tmp = dataref[NAV1] / 100.0;
    lcd.print(tmp);

    lcd.setCursor(0, 1);
    lcd.print("NAV2");
    if (channel == NAV2)
      lcd.print(">");
    lcd.setCursor(5, 1);
    tmp = dataref[NAV2] / 100.0;
    lcd.print(tmp);
  break;

  case COM1:
  case COM2:
    lcd.print("COM1");
    if (channel == COM1)
      lcd.print(">");
    lcd.setCursor(5, 0);
    tmp = dataref[COM1] / 100.0;
    lcd.print(tmp);

    lcd.setCursor(0, 1);
    lcd.print("COM2");
    if (channel == COM2)
      lcd.print(">");
    lcd.setCursor(5, 1);
    tmp = dataref[COM2] / 100.0;
    lcd.print(tmp);
  break;

  case ADF1:
  case ADF2:
    lcd.print("ADF1");
    if (channel == ADF1)
      lcd.print(">");
    lcd.setCursor(5, 0);
    lcd.print(dataref[ADF1]);

    lcd.setCursor(0, 1);
    lcd.print("ADF2");
    if (channel == ADF2)
      lcd.print(">");
    lcd.setCursor(5, 1);
    lcd.print(dataref[ADF2]);
  break;

  } // switch, print selected channels

  /////////////////
  // Print transponder code
  //
  lcd.setCursor(12, 0);
  // pad out to right if code has less than four digits
  if(dataref[XP_CODE] < 1000)
    lcd.print("0");
  if(dataref[XP_CODE] < 100)
    lcd.print("0");
  if(dataref[XP_CODE] < 10)
    lcd.print("0");
  lcd.print(dataref[XP_CODE]);

  /////////////////
  // Print transponder mode
  //
  lcd.setCursor(12, 1);
  switch (dataref[XP_MODE]) {

  case XP_OFF:
    lcd.print(" OFF");
  break;

  case XP_STBY:
    lcd.print("STBY");
  break;

  case XP_ON:
    lcd.print(" ON ");
  break;

  case XP_ALT:
    lcd.print(" ALT");
  break;

  case XP_TEST:
  default:
    lcd.print("TEST");
  break;

  } // switch, display transponder mode and code

  /////////////////
  // Transponder selection indication
  //
  // This is the > symbol next to the code or mode. Also the selected two
  // digits of the transponder code will flash.
  //
  switch(channel) {
  case XP_CODE:
    lcd.setCursor(11, 0);
    lcd.print(">");
    if(!flashNow) {
      lcd.print("  "); //overprint first two transponder-code characters
    }
  break;
  case XP_CODE_LO:
    lcd.setCursor(11, 0);
    lcd.print(">");
    if(!flashNow) {
      lcd.setCursor(14, 0);
      lcd.print("  "); //overprint last two transponder-code characters
    }
  break;
  case XP_MODE:
    lcd.setCursor(11, 1);
    lcd.print(">");
  break;
  } // switch, transponder selection indicator

} // displayUpdate

