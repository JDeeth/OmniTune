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

///////////////////
// Output hardware
enum LCD_PINS {
  RS = 39, RW, EN, D4, D5, D6, D7
};

LiquidCrystalFast lcd(RS, RW, EN, D4, D5, D6, D7);
// LCD pins: RS  RW  EN  D4 D5 D6 D7

void setupOutput() {
  pinMode (RS, OUTPUT);
  pinMode (RW, OUTPUT);
  pinMode (EN, OUTPUT);
  pinMode (D4, OUTPUT);
  pinMode (D5, OUTPUT);
  pinMode (D6, OUTPUT);
  pinMode (D7, OUTPUT);

  lcd.begin (16, 2);
  lcd.print("Hello world");

  pinmode (LED_BUILTIN, OUTPUT);
}

///////////////////
// Input hardware

// provisional plan: left up/down for coarse changes, right up/down for fine
// changes, left/right in for prev/next channel

enum INPUT_PINS {
  PIN_LEFT_UP = 4,
  PIN_LEFT_DOWN = 0,
  PIN_LEFT_IN = 16,

  PIN_RIGHT_UP = 12,
  PIN_RIGHT_DOWN = 8,
  PIN_RIGHT_IN = 17
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

///////////////////////////////////////////////////////////////////////////////
// X-Plane objects
// I am using ordinary integers, because I don't have a working X-Plane install

enum DATAREF_NAMES {
  NAV1, NAV2, ADF1, ADF2, COM1, COM2,
  DATAREF_COUNT
};

int dataref[DATAREF_COUNT];
//FlightSimInteger dataref[DATAREF_COUNT];

void setupDataref() {
  // commented for offline testing
  //dataref[NAV1] = XPlaneRef("sim/cockpit2/radios/actuators/nav1_frequency_hz");
  //dataref[NAV2] = XPlaneRef("sim/cockpit2/radios/actuators/nav2_frequency_hz");
  //dataref[ADF1] = XPlaneRef("sim/cockpit2/radios/actuators/adf1_frequency_hz");
  //dataref[ADF2] = XPlaneRef("sim/cockpit2/radios/actuators/adf2_frequency_hz");
  //dataref[COM1] = XPlaneRef("sim/cockpit2/radios/actuators/com1_frequency_hz");
  //dataref[COM2] = XPlaneRef("sim/cockpit2/radios/actuators/com2_frequency_hz");

  // dummy values for offline testing
  dataref[NAV1] = 11110;
  dataref[NAV2] = 11220;
  dataref[ADF1] = 345;
  dataref[ADF2] = 456;
  dataref[COM1] = 12330;
  dataref[COM2] = 12440;
}

///////////////////////////////////////////////////////////////////////////////
// Local objects

enum MODE_NAMES {
  mNAV1, mNAV2, mADF1, mADF2, mCOM1, mCOM2,
  MODE_COUNT
};

short mode = mNAV1;

//counter for flashing characters
int flashCount = 0;
bool flashNow = false;

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
  FlightSim.update();

  //display updated every 80ms (12.5fps)
  if (dispTimer > 80) {
    dispTimer -= 80;

    //increment character flashing counter
    //flashing sequence is Six On, Two Off, per display updates.
    ++flashCount;
    flashCount %= 8;
    if (flashCount < 6){
      flashNow = 1;
      digitalWrite(LED_BUILTIN, HIGH);
    }
    else {
      flashNow = 0;
      digitalWrite(LED_BUILTIN, LOW);
    }
  }
}
/*
  //--------------------------------------------------------------------------
  //Hardware input code

  //following code runs every Teensy loop

  //pressbutton on encoder shaft
  encPress.update();
  if(encPress.fallingEdge()) { //when the click-button is pressed
    //special case for leaving Transponder Code mode, going straight to Transponder Mode
    if(channel==6 && mode == 4 && xMode <=1) { //Xpdr Code channel, leaving 4th digit alter mode, and xpdr is not transmitting
      channel = 7; //go to Xpdr Mode channel
      mode = 1; //go to Xpdr Mode Alter mode
    }
    else
      ++mode %= 1 + channelModes[channel]; //increment mode and set to 0 if it's now outside the range for this channel
  }

  //encoder rotation
  //(necessary to check this as rapidly as possible)
  //(except perhaps if you use interrupts etc from more advanced parts of Encoders.h)
  int encDiff = (encTurn.read() - encPos) / 4; //enc.read changes by 4 for each detent, for the encoders I bought

  //if the encoder's been turned by one or more detents
  if (encDiff) {
    encReset();

    //reset flash counter (so digits will always be lit for at least 6 display update cycles after a digit is changed)
    //this means the digits will always be lit when you're turning the knob
    flashCount = 0;

    if (mode==0) {
      //MENU mode
      //change channel
      channel += encDiff; //encDiff can be negative

      //keep channel between 0 and numChannels-1
      while (channel < 0)            channel += numChannels;
      while (channel >= numChannels) channel -= numChannels;
    }
    else {
      //TUNE HI or TUNE LO mode
      //read frequency from X-Plane for the selected channel
      int freq = *channelRef[channel]; // ('freq' is a poor name for the transponder codes, but nevermind)

      switch (channel) {

      case 0: //nav1
      case 1: //nav2
        if (mode == 1) {
          //TUNE HI. Increment in megaherts, which is freq * 100 (dataref is in decakilohertz, decaKHz)
          freq += encDiff * 100;
          //crop to 108-118MHz range
          while (freq < 10800) freq += 1000;
          while (freq >= 11800) freq -= 1000;
        }
        if (mode >= 2) { //TUNE LO. Increment in decaKHz
          //remove MHz element from freq, leaving decaKHz element
          int mhz = freq / 100;
          freq -= mhz * 100;
          //increment freq in 50KHz (0.05MHz) steps
          freq += encDiff * 5;
          //crop freq to prevent TUNE LO mode from changing TUNE HI digits
          while (freq < 0) freq += 100;
          while (freq >= 100) freq -= 100;
          //reinstate MHz element
          freq += mhz * 100;
        }
        //write back to X-Plane using Teensyduino linker object
        *channelRef[channel] = freq;
      break;

      case 2: //adf1
      case 3: //adf2
        if (mode == 1) {
          //TUNE 1st Digit
          freq += encDiff * 100;
          while (freq < 100) freq += 900;
          while (freq >= 1000) freq -= 900;
        }
        if (mode == 2) {
          //TUNE 2nd Digit
          //remove 1st digit
          int digit1 = freq / 100;
          freq -= digit1 * 100;
          //increment 2nd digit
          freq += encDiff * 10;
          //crop to prevent changes to 1st digit
          while (freq < 0) freq += 100;
          while (freq >= 100) freq -= 100;
          //restore 1st digit
          freq += digit1 * 100;
          //NDB range is nominally 190-535KHz, but we'll allow 100-999KHz to be selected
          while (freq < 100) freq += 10;
        }
        if (mode >= 3) {
          //TUNE 3rd Digit
          //remove 1st and 2nd digits
          int digit12 = freq / 10;
          freq -= digit12 * 10;
          //increment freq in 1KHz steps
          freq += encDiff;
          //crop to prevent changes to 1st and 2nd digits
          while (freq < 0) freq += 10;
          while (freq >= 10) freq -= 10;
          //restore 1st and 2nd digits
          freq += digit12 * 10;
          //ensure next Mode is Menu
        }
        //write back to X-Plane via Teensyduino
        *channelRef[channel] = freq;
      break;

      case 4: //com1
      case 5: //com2
        if (mode == 1) {
          //TUNE HI. Much as NAV1/2 above
          freq += encDiff * 100;
          while (freq <  11800) freq += 1800; //laps round from 118.00 to 136.00
          while (freq >= 13600) freq -= 1800;
        }
        if (mode >= 2) {
          //TUNE LO
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
          ffreq += encDiff * 2.5;
          //convert back to integers (c++ drops the trailing .5 if necessary)
          freq = ffreq;
          //keep freq between 0 and <100 so this operation doesn't affect digits left of decimal point
          while (freq < 0) freq += 100;
          while (freq >= 100) freq -= 100;
          //reinstate the megahertz digits
          freq += mhz * 100;
        }
        //write updated frequency back to X-Plane
        *channelRef[channel] = freq;
      break;
      }
    }
  }
}

//----------------------------------------------------------------------------
//Display code

void dispRefresh() {
  // display-handling class takes char[] as input

  //This buffer will eventually be sent to the display-handling class to be displayed on the hardware
  char buf[9] = "        "; //8 spaces

  //read channel's dataref and convert it into char[] fsVal
  int fsInt = *channelRef[channel];
  char fsVal[6] = "     ";
  itoa (fsInt, fsVal, 10);

  const char Glyph_N1 = '\x88';
  const char Glyph_N2 = '\x89';
  const char Glyph_C1 = '\x8c';
  const char Glyph_C2 = '\x8d';

  switch (channel) { //identify selected channel on left side:
  case 0:  //nav1
    buf[0] = Glyph_N1;
  break;
  case 1:  //nav2
    buf[0] = Glyph_N2;
  break;
  case 2:  //adf1
    strcpy(&buf[0], "ADF1 ");
    if (mode) buf[4] = '>'; //if device is in 'alter frequency' mode, write "ADF1>456", otherwise have it as "ADF1 456"
  break;
  case 3:  //adf2
    strcpy(&buf[0], "ADF2 ");
    if (mode) buf[4] = '>';
  break;
  case 4:  //com1
    buf[0] = Glyph_C1;
  break;
  case 5:  //com2
    buf[0] = Glyph_C2;
  break;

  //for MHz radios, display frequency
  switch (channel) {
  //MHz frequencies
  case 0:  //nav1
  case 1:  //nav2
  case 4:  //com1
  case 5:  //com2
    if(mode) buf[1] = '>';
    strcpy(&buf[2], &fsVal[0]);
    if(mode==1 && !flashNow) strcpy(&buf[2], "   "); //flash digits left of decimal if they're selected for altering
    buf[5] = '.';
    strcpy(&buf[6], &fsVal[3]);
    if(mode==2 && !flashNow) strcpy(&buf[6], "  "); //flash digits right of decimal if they're selected for altering
  break;
    //ADF frequencies
  case 2:  //adf1
  case 3:  //adf2
    strcpy(&buf[5], &fsVal[0]);
    if(mode && !flashNow) buf[4+mode] = ' ';
  break;
  }

  //finally...
  //send buf to display-handling class, so it will be written on the display hardware
  disp.writeWord(buf);
}
*/

