//OmniTune
//(c) Jack Deeth 2012
//Read at your own risk!

//output libraries
#include <HDSP253x.h>  //a class I made for interfacing with a particular kind of obsolete LED matrix display
//input libraries
#include <Bounce.h>
#include <Encoder.h>

//----------------------------------------------------------------------------
//Hardware setup

//LED display
HDSP253x disp  (21,20,19,18, 17,16,15,14, 7,6,5,4, 3,2,1,0); 
/* HDSP253x pins:
 int RST, int FL, int A0, int A1,
 int A2,  int A3, int A4, int CE,
 int D0,  int D1, int D2, int D3,
 int D4,  int D5, int D6, int D7
 */
 
//rotary encoder
Encoder encTurn(9,8);
short encPos = 0;
void encReset() {
  encTurn.write(0); 
  encPos = 0;
}

//pushbutton
const short encPressPin = 13; //pushbutton on encoder shaft
Bounce encPress = Bounce (encPressPin, 8);

//----------------------------------------------------------------------------
//X-Plane objects

FlightSimInteger nav1; //teensyduino-specific X-Plane interface object
FlightSimInteger nav2;
FlightSimInteger adf1;
FlightSimInteger adf2;
FlightSimInteger com1;
FlightSimInteger com2;
FlightSimInteger xCode;
FlightSimInteger xMode;
FlightSimInteger audioMode;
FlightSimFloat flap;
FlightSimInteger times[3][3]; //first array: sim zulu/sim elapsed/system local. Second array: hour/minute/second

//----------------------------------------------------------------------------
//local objects
//(for the UI and processing on board the Teensy)

elapsedMillis dispTimer = 0; //to avoid updating display too frequently
elapsedMillis fsTimer = 0;  //to avoid congesting X-Plane link by calling too frequently

//counter for flashing characters
int flashCount = 0;
short unsigned flashOnNow = 0; // 0: flashing characters off; 1: flashing characters on

//channel information
//channel sequence:
//nav1,2, adf1,2, com1,2, xpdr code,mode, ident select, sim UTC, sim elapsed time, PC local time, flap position
short channel = 0; //currently selected channel. 0:nav1, 1:nav2 etc
const short numChannels = 13; 
FlightSimInteger* channelRef[13] = { //references to datarefs used by each mode.
  &nav1,&nav2,
  &adf1,&adf2,
  &com1,&com2,
  &xCode, &xMode,
  &audioMode, 0, //here is where 'feature creep' comes in. channelRef really ought to be replaced with something else!
  0, 0,
  0};

//mode information
//mode sequence:
//0: menu, 1: tune hi/1st digit, 2: tune lo/2nd digit, 3: tune 3rd digit etc...
unsigned short mode = 0; //currently selected mode
const short channelModes[13] = { //number of modes in each channel (not including 'menu' mode)
  2,2,  //nav
  3,3,  //adf
  2,2,  //com
  4,1,  //xpdr code and mode
  1,0,  //audio channel select, sim zulu time,
  0,0,  //sim elapsed time, system time
  1};   //flaps

//----------------------------------------------------------------------------
//Setup function
void setup() {
  //set up pushbutton
  pinMode(encPressPin, INPUT_PULLUP);
  
  //set up encoder
  encReset();
 
  //assign all datarefs
  nav1 = XPlaneRef("sim/cockpit2/radios/actuators/nav1_frequency_hz");
  nav2 = XPlaneRef("sim/cockpit2/radios/actuators/nav2_frequency_hz");
  adf1 = XPlaneRef("sim/cockpit2/radios/actuators/adf1_frequency_hz");
  adf2 = XPlaneRef("sim/cockpit2/radios/actuators/adf2_frequency_hz");
  com1 = XPlaneRef("sim/cockpit2/radios/actuators/com1_frequency_hz");
  com2 = XPlaneRef("sim/cockpit2/radios/actuators/com2_frequency_hz");
  xCode = XPlaneRef("sim/cockpit2/radios/actuators/transponder_code");
  xMode = XPlaneRef("sim/cockpit/radios/transponder_mode");
  audioMode = XPlaneRef("sim/cockpit2/radios/actuators/audio_nav_selection");
  flap = XPlaneRef("sim/cockpit2/controls/flap_ratio");

  times[0][0] = XPlaneRef("sim/cockpit2/clock_timer/zulu_time_hours");
  times[0][1] = XPlaneRef("sim/cockpit2/clock_timer/zulu_time_minutes");
  times[0][2] = XPlaneRef("sim/cockpit2/clock_timer/zulu_time_seconds");
  times[1][0] = XPlaneRef("sim/cockpit2/clock_timer/elapsed_time_hours");
  times[1][1] = XPlaneRef("sim/cockpit2/clock_timer/elapsed_time_minutes");
  times[1][2] = XPlaneRef("sim/cockpit2/clock_timer/elapsed_time_seconds");
  times[2][0] = XPlaneRef("Dozer/systime/local_hours"); //from a plugin I wrote, which finds the PC's idea of local time
  times[2][1] = XPlaneRef("Dozer/systime/local_minutes");
  times[2][2] = XPlaneRef("Dozer/systime/local_seconds");
}

//----------------------------------------------------------------------------
//Loop function
void loop() {
  //fs link updated every 40ms (25hz)
  if (fsTimer > 40) { 
    FlightSim.update(); 
	//this is bad practice; FlightSim.update() should be called as often as 
	//possible and another method should be used to avoid choking the USB
	//(not that I've encountered a saturated USB so far!)
    fsTimer -= 40;
  }

  //display updated every 80ms (12.5fps)
  if (dispTimer > 80) { 
    dispTimer -= 80;
	
    //displays shows "OmniTune" if X-Plane is not running
    FlightSim.isEnabled()? dispRefresh(): disp.writeWord("OmniTune");

	//increment character flashing counter
	//flashing sequence is Six On, Two Off, per display updates.
    flashCount++;
    if (flashCount > 8) {
      flashCount = 0;
    }
    if (flashCount < 6){
      flashOnNow = 1;
      digitalWrite(LED_BUILTIN, HIGH);
    } 
    else {
      flashOnNow = 0;
      digitalWrite(LED_BUILTIN, LOW);
    }
  }

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

      case 6:
        //Transponder Code change mode
        //we will put xpdr into stby mode when changing code, if it is transmitting
        xMode > 1? xMode = 1: 0;
        //it is necessary to convert transponder dataref from decimal to octal (no 8 or 9 digits)
        //this is /maybe/ reinventing the wheel
		
		//separate decimal integer into four separate integers, one for each xpdr digit
        short code[4]; 
        code[0] = freq/1000;
        code[1] = (freq - 1000*(freq/1000))/100;
        code[2] = (freq - 100*(freq/100))/10;
        code[3] = freq - 10*(freq/10);
		
        if (mode >= 1 || mode <= 4) { //should always be true
          //alter selected digit
          code[mode-1] += encDiff;
		  //trim selected digit to sit between 0-7 inclusive
          while (code[mode-1] < 0 ) code[mode-1] += 8;
          while (code[mode-1] >= 8) code[mode-1] -= 8;
        }
		//recombine individual digits and send back to X-Plane
        *channelRef[channel] = code[0]*1000 + code[1]*100 + code[2]*10 + code[3];
        break;

      case 7: //xpdr change mode
        //simple this one
        freq += encDiff;
        freq < 0? freq = 0: 0;
        freq > 5? freq = 5: 0;
        *channelRef[channel] = freq;
        break;

      case 8: //nav ident selection
        //dataref reads as: 0:NAV1, 1:NAV2, 2:ADF1, 3:ADF2, 9:None
        if(freq == 9) {
		  freq = 4; // now we can use mod5 arithmetic
		}
		//change which radio has audio turned on
        freq += encDiff;
        while (freq < 0) freq += 5;
        while (freq > 4) freq -= 5;
		//put freq back into format expected by dataref
        if(freq == 4) {
		  freq = 9;
		}
		//write back to X-Plane
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
  case 6:  //xpdr code
    strcpy(buf, "Xpr 0000");
    if (mode) buf[3] = '>'; 
    { //scope for xcShift
	  //shift xpdr digits to the right if the value's below 1000
      int xcShift = 0;
      if(fsInt < 1000) xcShift++;
	  if(fsInt < 100)  xcShift++;
	  if(fsInt < 10)   xcShift++;
      strcpy(&buf[4+xcShift], &fsVal[0]);
    }
    if(mode && !flashOnNow) buf[3+mode] = ' '; //blank out the selected digit if the flashing-digit timer calls for it
    break;
  case 7: //xpdr mode
    strcpy(buf, "Xpr     ");
    if (mode) buf[3] = '>'; 
    switch(fsInt) {
    case 0: 
      strcpy(&buf[5],"Off"); 	//"Xpdr Off"
      break;
    case 1: 
      strcpy(&buf[4],"Stby");	//"XpdrStby"
      break;
    case 2: 
      strcpy(&buf[6],"On");		//"Xpdr  On"
      break;
    case 3: 
      strcpy(&buf[5],"Alt");	//"Xpdr Alt"
      break;
    default: 
      strcpy(&buf[4],"Test");	//"XpdrTest"
      break;
    }
    break;
  case 8: //radio ident selection
    strcpy(&buf[0],"ID      ");
    if (mode) buf[2] = '>'; 
    switch(fsInt) {
    case 0: 
      strcpy(&buf[4],"NAV1");
      break;
    case 1: 
      strcpy(&buf[4],"NAV2");
      break;
    case 2: 
      strcpy(&buf[4],"ADF1");
      break;
    case 3: 
      strcpy(&buf[4],"ADF2");
      break;
    default: 
      strcpy(&buf[4],"None");
      break;
    }
    break;
  case 9: //clocks
  case 10:
  case 11:
    {
	  //note: if I was writing this again, I'd do it completely differently, using streams rather
	  //than this nonsense
	  
	  //setup buffers for hh/mm/ss digit characters
      char hh[3] = "00";
      char mm[3] = "00";
      char ss[3] = "00";
	  //read time from X-Plane/my system time plugin, and convert it from integer to characters
      itoa (times[channel-9][0], hh, 10);
      itoa (times[channel-9][1], mm, 10);
      itoa (times[channel-9][2], ss, 10);
	  //write hh/mm/ss into another buffer
      strcpy(&buf[0], "00:00:00");
	  //            |--------------------------| this bit is to right-justify the digits in each field
      strcpy(&buf[( times[channel-9][0]<10? 1: 0 )],hh);
      strcpy(&buf[( times[channel-9][1]<10? 4: 3 )],mm);
      strcpy(&buf[( times[channel-9][2]<10? 7: 6 )],ss);
	  //reinstate first dividing colon (overwritten by char hh[3]'s terminating null character)
      buf[2] = ':';
	  //use signifying letter as second divider
      if (channel == 9) {
        buf[5] = 'z'; //z for zulu-time (UTC)
		//if the button's pressed, display "ZuluTime" on display
        encPress.read()? 0: strcpy(&buf[0],"ZuluTime");
      }
      if (channel == 10) {
        buf[5] = 'e'; //e for Elapsed
        encPress.read()? 0: strcpy(&buf[0],"Elapsed ");
      }
      if (channel == 11) {
        buf[5] = 's'; //s for System (as in, PC system)
        encPress.read()? 0: strcpy(&buf[0],"Sys Time");
      }
    }
    break;
  case 12: //display flap angle (for DH Comet, which has 80° of flap)
    strcpy(&buf[0], "Flap:   ");
    int flapDeg = 80.0 * flap; //convert from flap deployment ratio to degrees
    char flapChar[3] = "00";
    itoa(flapDeg,flapChar, 10); //convert numbers to characters
    strcpy(&buf[6], flapChar);
    if (!flapDeg) strcpy(&buf[6], "00"); //if flaps are retracted, display "Flap: 00" (workaround)
    break;
  }

  //for MHz radios, display frequency 
  switch (channel) {
    //MHz frequencies  
  case 0:  //nav1
  case 1:  //nav2
  case 4:  //com1
  case 5:  //com2
    if(mode) buf[1] = '>';
    strcpy(&buf[2], &fsVal[0]);
    if(mode==1 && !flashOnNow) strcpy(&buf[2], "   "); //flash digits left of decimal if they're selected for altering
    buf[5] = '.';
    strcpy(&buf[6], &fsVal[3]);
    if(mode==2 && !flashOnNow) strcpy(&buf[6], "  "); //flash digits right of decimal if they're selected for altering
    break;
    //ADF frequencies
  case 2:  //adf1
  case 3:  //adf2
    strcpy(&buf[5], &fsVal[0]);
    if(mode && !flashOnNow) buf[4+mode] = ' ';
    break;
  }

  //finally...
  //send buf to display-handling class, so it will be written on the display hardware
  disp.writeWord(buf);
}
