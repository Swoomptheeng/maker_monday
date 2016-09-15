#include <Wire.h>
#include "Adafruit_MPR121.h"
// You can have up to 4 on one i2c bus but one is enough for testing!
Adafruit_MPR121 cap = Adafruit_MPR121();

//TOUCH

// Keeps track of the last pins touched
// so we know when buttons are 'released'
uint16_t lasttouched = 0;
uint16_t currtouched = 0;

//MIDI note array
int MIDINoteArr[] = {54,55,56,57,58,59,60,61,62,63,64,65};

void senseTouch()
{
 //CAPACITATIVE TOUCH
  // Get the currently touched pads
  currtouched = cap.touched();
  
  
  for (uint8_t i=0; i<12; i++) {
    
    int pitch = MIDINoteArr[i];
      
    // it if *is* touched and *wasnt* touched before, alert!
    if ((currtouched & _BV(i)) && !(lasttouched & _BV(i)) ) {
      //Serial.print(i); Serial.println(" touched");
      //MIDI - NOTE ON
      Serial.write(0x95); //NOTE ON & Channel Num
      Serial.write(pitch); //Pitch
      Serial.write(100); // Velocity
    }
    // if it *was* touched and now *isnt*, alert!
    if (!(currtouched & _BV(i)) && (lasttouched & _BV(i)) ) {
      //Serial.print(i); Serial.println(" released");
      Serial.write(0x85);
      Serial.write(pitch);
      Serial.write(0);
    }
  }

  // reset our state
  lasttouched = currtouched; 
}









//SOUND
const int MIC_PIN = A0; // select the input pin for the potentiometer
const int NOISE_LEVEL = 10;
const int NOISE_MAX = 13;
int mic_input = 1; // variable to store the value coming from the sensor
int previous_mic_input = 0;
int floating_midi_val = NOISE_LEVEL;
int last_floating_midi_val = floating_midi_val;

void senseSound ()
{  
  mic_input = analogRead (MIC_PIN);

  int voice = abs(mic_input - previous_mic_input);
  voice = constrain(voice, 0, NOISE_MAX);  //ARBITRARY MAX INPUT USED HERE OF 13
  voice = map(voice, 0, NOISE_MAX, 0, 127); //then map the input to 0-127 - your CC MIDI val
  
  //If a louder noise is heard... push up the val
  if (voice > floating_midi_val && voice > NOISE_LEVEL)
  {
    floating_midi_val = voice;
  }
  
  //print to screen display
  /*String c = "x";
  for (int i=0; i<floating_midi_val; i++)
  {
    c += "x";
  }
  Serial.println (c);
  Serial.print("floating_midi_val :"); Serial.print(floating_midi_val);
  Serial.print(", voice : "); Serial.print(voice);
  Serial.print(", mic_input : "); Serial.println(mic_input);
  */
  //MIDI
  if (floating_midi_val > 0)
  {
    Serial.write(0xB5); //CC & Channel Num
    Serial.write(0x02); //Controller Num
    Serial.write(floating_midi_val); // Value
    floating_midi_val-=10; //decrease gradually the value until it's back at the noise level 
  }
  
  last_floating_midi_val = floating_midi_val;
  previous_mic_input = mic_input;
}







//FLEX
const int FLEX_PIN = A2; // Pin connected to voltage divider output
const int FLEX_AT_REST = 10;
// Measure the voltage at 5V and the actual resistance of your
// 47k resistor, and enter them below:
const float VCC = 4.98; // Measured voltage of Ardunio 5V line
const float R_DIV = 47500.0; // Measured resistance of 3.3k resistor
// Upload the code, then try to adjust these values to more
// accurately calculate bend degree.
const float STRAIGHT_RESISTANCE = 37300.0; // resistance when straight
const float BEND_RESISTANCE = 90000.0; // resistance at 90 deg

void senseFlex()
{
  //FLEX
  // Read the ADC, and calculate voltage and resistance from it
  int flexADC = analogRead(FLEX_PIN);
  float flexV = flexADC * VCC / 1023.0;
  float flexR = R_DIV * (VCC / flexV - 1.0);
  //Serial.println("Resistance: " + String(flexR) + " ohms");

  // Use the calculated resistance to estimate the sensor's
  // bend angle:
  float angle = map(flexR, STRAIGHT_RESISTANCE, BEND_RESISTANCE,
                   0, 90.0);
  //Serial.println("Bend: " + String(angle) + " degrees");
  //Serial.println();
  
  //MIDI
  angle = constrain(angle, 50.0, 70.0);
  int val = map((int) angle, 50, 70, 0, 127);    //50 here is natural "unsqueezed" angle and 70 full compression
  if (val > FLEX_AT_REST)
  {
    Serial.write(0xB5); //CC & Channel Num
    Serial.write(0x01); //Controller Num
    Serial.write(val); // Value
  } 
}


void setup () 
{
  while (!Serial);        // needed to keep leonardo/micro from starting too fast!
  
  Serial.begin (115200);
  
  if (!cap.begin(0x5A)) {
    Serial.println("MPR121 not found, check wiring?");
    while (1);
  }
  
  pinMode(FLEX_PIN, INPUT);
  pinMode(MIC_PIN, INPUT);
}



void loop () 
{
  senseSound();

  senseTouch();
  
  senseFlex();
  
  delay (50);
}
