///////////////////////////////////////////////////////////////////////////////
//
// OmniTune (incomplete)
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
  NAV1, NAV2, COM1, COM2, ADF1, ADF2,
  DATAREF_COUNT
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

  // dummy values for offline testing
  dataref[NAV1] = 11110;
  dataref[NAV2] = 11220;
  dataref[COM1] = 12330;
  dataref[COM2] = 12440;
  dataref[ADF1] = 345;
  dataref[ADF2] = 456;
}

///////////////////////////////////////////////////////////////////////////////
// Local objects

short channel = NAV1; // indicates selected channel

//counter for flashing characters
//int flashCount = 0;
//bool flashNow = false;

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
    //flashing sequence is Six On, Two Off, per display updates.
    //++flashCount %= 8; // increment flashCount between 0 and 7
    //flashNow = (flashCount < 6);
    //digitalWrite(LED_BUILTIN, flashNow);
    // on reflection, flashing is not needed with this hardware config

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

    switch (channel) {

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

    }

    dataref[channel] = freq;
  }
}

///////////////////////////////////////////////////////////////////////////////
// Display Update
//
// Reads datarefs and draws selected channels onto display
//
void displayUpdate() {

  lcd.clear();
  lcd.setCursor(0, 0);

  float tmp;

  switch (channel) {
  case NAV1:
  case NAV2:
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

  }
}

