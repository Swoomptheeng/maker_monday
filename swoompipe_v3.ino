#define DEBUG false //Debugging - set to true to output to serial, or false to send midi

#include <Wire.h>
#include "Adafruit_MPR121.h"
#include <SoftwareSerial.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>

/* Assign a unique ID to this sensor at the same time */
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

// You can have up to 4 on one i2c bus but one is enough for testing!
Adafruit_MPR121 cap = Adafruit_MPR121();

//TOUCH
uint16_t lasttouched = 0;
uint16_t currtouched = 0;

//MIDI
//CHANNEL NUMS
#define NOTE_ON_CHNUM 0x95
#define NOTE_OFF_CHNUM 0x85
//
#define DRONE_KEY 50
//CONTROLLER NUMS
#define CC_CHNUM 0xB5
#define MIC_CONTROLLER_NUM 0x02
//#define FLEX_CONTROLLER_NUM 0x01
#define PITCH_CONTROLLER_NUM 0x03
#define ROLL_CONTROLLER_NUM 0x04

#define NUM_OF_KEYS 8
//60 == c4 (middle C)
//int MIDINoteArr[] = {54,55,56,57,58,59,60,61,62,63,64,65};
// A ,A#,C , D , D# , F , G , A - CAN USE ADDITIONAL A or G - USE D as Drone

int MIDINoteArr[] = {57,58,60,62, 63,65,67,69};// Key For Demo at Maker MOnday Showcase
#define midiNoteR1 72
#define midiNoteR2 73
#define midiNoteR3 74
#define midiNoteR4 75
//
float airInBag = 0;
float prevAirInBag = 0;

//PINS
#define MIC_PIN A0
#define REED1_PIN 5
#define REED2_PIN 6
#define BT_RECEIVE_PIN 10
#define BT_TRANSMIT_PIN 11

//BLUETOOTH
SoftwareSerial conn(BT_RECEIVE_PIN, BT_TRANSMIT_PIN); // RX, TX

void senseTouch()
{
  //CAPACITATIVE TOUCH
  // Get the currently touched pads
  currtouched = cap.touched();
  
  for (uint8_t i=0; i<NUM_OF_KEYS; i++) 
  {
    int pitch = MIDINoteArr[i];
 
    // NOTE ON
    if (((currtouched & _BV(i)) && !(lasttouched & _BV(i)))  &&  airInBag > 0) 
    {
      //MIDI - NOTE ON
      if (DEBUG)
      {
        conn.print(i); conn.println(" touched");
      }
      else
      {
        conn.write(NOTE_ON_CHNUM); //NOTE ON & Channel Num
        conn.write(pitch); //Pitch
        conn.write(127); // Velocity
      }
    }
 
    // NOTE OFF
    if ((!(currtouched & _BV(i)) && (lasttouched & _BV(i))) ||  (airInBag == 0 && prevAirInBag != 0)) 
    {
      if (DEBUG)
      {
        conn.print(i); conn.println(" released");
      }
      else
      {
        //THESE ARE GETTING SENT BUT NO NOTE ON IF NO AIR
        conn.write(NOTE_OFF_CHNUM); // NOTE OFF
        conn.write(pitch);
        conn.write(1);//THIS WAS SET TO 0 BUT THIS CAUSES  ERROR - SHOULDN'T MATTER THOUGH AS IT'S A NOTE OFF MESSAGE BEING SENT
      }
    }
  }

  // reset our state
  lasttouched = currtouched; 
}










//MIC
const int buffer_size = 10;
int noises[buffer_size];
int cur_index = 0;
//unsigned long last_shift = 0;
unsigned long mic_max = 0;
unsigned long mic_min = 99999999;
#define MIC_TOLERANCE 1
#define FALL_RATE 0.5
//MIDI
int last_MIDI_val = 0;
int MIDI_val = 1;


void senseSound ()
{  
  // read the value from the sensor:
  noises[cur_index] = analogRead(MIC_PIN);
  cur_index = (cur_index + 1) % buffer_size;
 // if (millis() - last_shift > 100){    //BEN - DEACTIVATED AS A DELAY IS SET IN THE LOOP
     //last_shift = millis();
     unsigned int avg = average(noises, buffer_size);
     unsigned long mic = variance(noises, buffer_size);
     mic_max = max(mic, mic_max);
     mic_min = min(mic, mic_min);
     MIDI_val = map(mic, mic_min, mic_max, 0, 127); //then map the input to 0-127 - your CC MIDI val
  //}
  
  //MIDI
  prevAirInBag = airInBag;
  int diff = abs(MIDI_val - last_MIDI_val);
  if (MIDI_val != last_MIDI_val && diff > MIC_TOLERANCE)
  {
    if (DEBUG)
    {
      conn.print("Volume:");
      conn.print(mic);
      conn.print("   ~    Raw input:");
      conn.print(noises[cur_index]);
      conn.print(", MIDI_val :"); conn.println(MIDI_val);
      
      //Print a row of xs as a volume meter
      String c = "x";
      for (int i=0; i<MIDI_val; i++)
      {
        c += "x";
      }
      conn.println (c);
    }
    else
    {
      airInBag += MIDI_val;
      if (airInBag>127) {airInBag = 127;}
    }
    
    last_MIDI_val = MIDI_val;
  }
  
  if (airInBag>0)
  {
    if (prevAirInBag == 0)
    {
      if(DEBUG)
      {
         conn.println("NEW AIR! NOTE ON");
      }
      else
      {
        conn.write(NOTE_ON_CHNUM);
        conn.write(DRONE_KEY);
        conn.write(127);
      }
    }
    
    if (DEBUG)
    {
      conn.print("Air left in bag :"); conn.println(airInBag);
    }
    else
    {
      conn.write(CC_CHNUM); //CC & Channel Num
      conn.write(MIC_CONTROLLER_NUM); //Controller Num
      int airMIDIVal = (int)airInBag;
      conn.write(airInBag); // Value
    }
    
    airInBag -= FALL_RATE; //decrease gradually
    if (airInBag<=0)
    {
      if (prevAirInBag > 0)
      {
        if(DEBUG)
        {
           conn.println("OUT OF AIR! NOTE OFF");
        }
        else
        {
          conn.write(NOTE_OFF_CHNUM); // NOTE OFF
          conn.write(DRONE_KEY);
          conn.write(1);
        }
      }
       airInBag = 0; 
    }
  }
}

int average(int* array, int length){
  int sum = 0;
  int i;
  for(i = 0; i < length ; i++){
    sum += array[i];
  }
  int avg = sum / length ;
  return avg;
}

unsigned long variance(int* array, int length){
  long sum = 0;
  long avg = average(array, length);
  for(int i = 0; i < length ; i++){
    sum += (array[i] - avg)*(array[i] - avg);
  }
  unsigned long mic = sum / length;
  return mic;	
}


int r1 = 0;
int r2 = 0;
int r3 = 0;
int r4 = 0;

int oldR1 = 0;
int oldR2 = 0;
int oldR3 = 0;
int oldR4 = 0;

void updateReedVars()
{
  r1 = 1 - digitalRead(REED1_PIN); //PULLED HIGH BY PULLUP RESISTOR SO 1- BIT IS TO NORMALISE (1 = on)
  r2 = 1 - digitalRead(REED2_PIN);
  
  if (r1 + r2 == 0) {r4 = 1;} else {r4 = 0;}  //both off
  if (r1 + r2 == 2) {r1 = 0; r2 = 0; r3 = 1;} else {r3 = 0;} //both on //MUST BE AFTER LINE ABOVE!
}

void senseReeds ()
{
  /* CODE IS CRUDE AND LAZY - NEEDS CONDENSING! (TO DO)*/
  updateReedVars();
  
  if (r3 == 0)
  {
    if (r1 != oldR1)
    {
      if (r1 == 1)
      {
        if (DEBUG)
        {
          conn.print(r1);
          conn.print(r2);
          conn.print(r3);
          conn.println(r4);
        }
        else
        {
          conn.write(NOTE_ON_CHNUM); // NOTE ON
          conn.write(midiNoteR1);
          conn.write(127);
        }
      }
      else
      {
        if (DEBUG)
        {
          conn.print(r1);
          conn.print(r2);
          conn.print(r3);
          conn.println(r4);
        }
        else
        {
          conn.write(NOTE_OFF_CHNUM); // NOTE OFF
          conn.write(midiNoteR1);
          conn.write(1);
        }
      }
      oldR1 = r1;
    }
  
    if (r2 != oldR2)
    {
      if (r2 == 1 && r3 == 0)
      {
        if (DEBUG)
        {
          conn.print(r1);
          conn.print(r2);
          conn.print(r3);
          conn.println(r4);
        }
        else
        {
          conn.write(NOTE_ON_CHNUM); // NOTE ON
          conn.write(midiNoteR2);
          conn.write(127);
        }
      }
      else
      {
        if (DEBUG)
        {
          conn.print(r1);
          conn.print(r2);
          conn.print(r3);
          conn.println(r4);
        }
        else
        {
          conn.write(NOTE_OFF_CHNUM); // NOTE OFF
          conn.write(midiNoteR2);
          conn.write(1);
        }
      }
      oldR2 = r2;
    }
  }

  if (r3 != oldR3)
  {
    if (r3 == 1)
    {
      if (DEBUG)
      {
        conn.print(r1);
        conn.print(r2);
        conn.print(r3);
        conn.println(r4);
      }
      else
      {
        conn.write(NOTE_ON_CHNUM); // NOTE ON
        conn.write(midiNoteR3);
        conn.write(127);
      }
    }
    else
    {
      if (DEBUG)
      {
        conn.print(r1);
        conn.print(r2);
        conn.print(r3);
        conn.println(r4);
      }
      else
      {
        conn.write(NOTE_OFF_CHNUM); // NOTE OFF
        conn.write(midiNoteR3);
        conn.write(1);
      }
    }
    oldR3 = r3;
  }

  if (r4 != oldR4)
  {
    if (r4 == 1)
    {
      if (DEBUG)
      {
        conn.print(r1);
        conn.print(r2);
        conn.print(r3);
        conn.println(r4);
      }
      else
      {
        conn.write(NOTE_ON_CHNUM); // NOTE ON
        conn.write(midiNoteR4);
        conn.write(127);
      }
    }
    else
    {
      if (DEBUG)
      {
        conn.print(r1);
        conn.print(r2);
        conn.print(r3);
        conn.println(r4);
      }
      else
      {
        conn.write(NOTE_OFF_CHNUM); // NOTE OFF
        conn.write(midiNoteR4);
        conn.write(1);
      }
    }
    oldR4 = r4;
  }
}



void setup () 
{
  conn.begin (9600);
  
  if (DEBUG)
  {
    conn.println("Started bluetooth connection");
  }
    
  if (!cap.begin(0x5A)) {
    conn.println("MPR121 not found, check wiring?");
    while (1);
  }
  
  /* Initialise the accelerometer */
  if(!accel.begin())
  {
    /* There was a problem detecting the ADXL345 ... check your connections */
    conn.println("ADXL345 not found, check wiring!");
    while(1);
  }
  accel.setRange(ADXL345_RANGE_16_G);
  sensor_t sensor;
  accel.getSensor(&sensor);
  
  for (int i = 0 ; i < buffer_size; i++) noises[i] = 512;//init noises array
  
  pinMode(13, OUTPUT);
  pinMode(MIC_PIN, INPUT);
  pinMode(REED1_PIN, INPUT_PULLUP);
  pinMode(REED2_PIN, INPUT_PULLUP);
  
  updateReedVars(); //init vars here
  
  delay(500);
}




int pitch_midi_val = 0;
int roll_midi_val = 0;
int last_pitch_midi_val = 127;
int last_roll_midi_val = 127;
  
void senseTilt()
{
  /* Get a new sensor event */ 
  sensors_event_t event; 
  accel.getEvent(&event);
  
  //These seem to be a max of +/-90 degrees
  int roll = 180 * atan (event.acceleration.z/sqrt(event.acceleration.y*event.acceleration.y + event.acceleration.z*event.acceleration.x))/M_PI;
  int pitch = 180 * atan (event.acceleration.y/sqrt(event.acceleration.x*event.acceleration.x + event.acceleration.z*event.acceleration.z))/M_PI;
  pitch_midi_val = map(pitch, -90, 90, 0, 127);
  roll_midi_val = map(roll, -90, 90, 0, 127);
  
  int pitch_tolerance = 2;
  int roll_tolerance = 2;
  
  /* Display the results (acceleration is measured in m/s^2) */
  //conn.print("X: "); conn.print(event.acceleration.x); conn.print("  ");
  //conn.print("Y: "); conn.print(event.acceleration.y); conn.print("  ");
  //conn.print("Z: "); conn.print(event.acceleration.z); conn.print("  "); conn.println("m/s^2 ");
 
  //PITCH
  if (pitch_midi_val != last_pitch_midi_val && abs(pitch_midi_val - last_pitch_midi_val) > pitch_tolerance)
  {
    //CONTROL CHANGE DATA
    if (DEBUG)
    {
      conn.print("Pitch: "); conn.print(pitch);
      conn.print(" MIDI: "); conn.println(pitch_midi_val);
    }
    else
    {
      conn.write(CC_CHNUM); //CC & Channel Num
      conn.write(PITCH_CONTROLLER_NUM); //Controller Num
      conn.write(pitch_midi_val); // Value
    }

    last_pitch_midi_val = pitch_midi_val;
  }
  
  //ROLL
  if (roll_midi_val != last_roll_midi_val && abs(roll_midi_val - last_roll_midi_val) > roll_tolerance)
  {
    if (DEBUG)
    {
      conn.print("Roll: "); conn.print(roll);
      conn.print(" MIDI: "); conn.print(roll_midi_val); conn.println("");
    }
    else
    {
      conn.write(CC_CHNUM); //CC & Channel Num
      conn.write(ROLL_CONTROLLER_NUM); //Controller Num
      conn.write(roll_midi_val); // Value
    }
    last_roll_midi_val = roll_midi_val;
  }
}

void loop () 
{
    senseSound();
  
    senseTouch();
    
    senseTilt();

    senseReeds();
    
  delay (50);
}
