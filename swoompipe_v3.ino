#include <Wire.h>
#include "Adafruit_MPR121.h"
#include <SoftwareSerial.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>

//PINS
#define MIC_PIN A0
#define REED1_PIN 5
#define REED2_PIN 6
#define BT_RECEIVE_PIN 10
#define BT_TRANSMIT_PIN 11

//BLUETOOTH
SoftwareSerial conn(BT_RECEIVE_PIN, BT_TRANSMIT_PIN); // RX, TX

//OPTIONS

#define DEBUG false // Debugging - set to true to output to serial, or false to send midi
                    // REMEMBER TO USE TERA TERM AS THE MESSAGE WINDOW NOT THE ARDUINO ONE!!
boolean pitch_active = true;
boolean roll_active = true;
boolean keys_require_air = false; //SETTINGS --- ARE KEYS LOCKED WHEN "BAG IS EMPTY"?
boolean play_drone_on_blow = false; //SETTINGS --- PLAY DRONE NOTE ON FRESH BLOW INTO EMPTY BAG?
boolean reedsInCCMode = false;

                    
//ROLL PITCH ACCELEROMETER
int pitch_midi_val = 0;
int roll_midi_val = 0;
int last_pitch_midi_val = 127;
int last_roll_midi_val = 127;
#define pitch_tolerance 2 //higher number mean less sensitivity (1 is very sensitive and sends out a lot of MIDI)
#define roll_tolerance 2

//TOUCH KEYS
/* Assign a unique ID to this sensor at the same time */
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

// You can have up to 4 on one i2c bus but one is enough for testing!
Adafruit_MPR121 cap = Adafruit_MPR121();

uint16_t lasttouched = 0;
uint16_t currtouched = 0;
#define NUM_OF_KEYS 8


//MIDI
//60 == c4 (middle C)
//int MIDINoteArr[] = {54,55,56,57,58,59,60,61,62,63,64,65}; // Default incremental key scale
//Notes from Paul on keys for track - A ,A#,C , D , D# , F , G , A - CAN USE ADDITIONAL A or G - USE D as Drone
int MIDINoteArr[] = {57,58,60,62, 63,65,67,69};// Keys For Pauls Music Demo at Maker Monday Showcase

//  CHANNEL NUMS
#define NOTE_ON_CHNUM 0x95
#define NOTE_OFF_CHNUM 0x85

//  CONTROLLER NUMS
#define CC_CHNUM 0xB5
#define MIC_CONTROLLER_NUM 0x02
#define PITCH_CONTROLLER_NUM 0x03
#define ROLL_CONTROLLER_NUM 0x04

//REEDS

//REED CC CONTROLLERS (IF USED)
#define REED_CONTROLLER_NUM1 0x05
#define REED_CONTROLLER_NUM2 0x06
#define REED_CONTROLLER_NUM3 0x07
#define REED_CONTROLLER_NUM4 0x08
//REED NOTE ON NOTES (IF USED)
#define REED_NO_NOFF_NUM1 72
#define REED_NO_NOFF_NUM2 73
#define REED_NO_NOFF_NUM3 74
#define REED_NO_NOFF_NUM4 75


//MIC
#define MIC_TOLERANCE 1
#define FALL_RATE 0.5

float airInBag = 0;
float prevAirInBag = 0;

const int buffer_size = 10;
int noises[buffer_size];
int cur_index = 0;
unsigned long mic_max = 0;
unsigned long mic_min = 99999999;

//  MIC MIDI
#define DRONE_KEY 50
int last_MIDI_airVal = 0;
int MIDI_airVal = 1;



void setup () {
  conn.begin (9600);
  
  /*
  //DONT USE THIS DOESNT WORK WITH BLUETOOTH CONNECTION
  while (!Serial) { // needed to keep leonardo/micro from starting too fast!
    delay(10);
  }
  */
  
  if (DEBUG) conn.println("Started bluetooth connection"); 
    
  if (!cap.begin(0x5A)) {
    conn.println("MPR121 not found, check wiring?");
    while (1);
  }
  
  /* Initialise the accelerometer */
  if(!accel.begin()) {
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
  
  delay(500);
}

void senseTouch() {
  //CAPACITATIVE TOUCH
  currtouched = cap.touched(); // Get the currently touched pads
  
  for (uint8_t i=0; i<NUM_OF_KEYS; i++) {
    int pitch = MIDINoteArr[i];
 
    // NOTE ON
    if (((currtouched & _BV(i)) && !(lasttouched & _BV(i)))  &&  (airInBag > 0 || keys_require_air == false)) {
      //MIDI - NOTE ON
      if (DEBUG){ conn.print(i); conn.println(" touched"); }
      else {
        conn.write(NOTE_ON_CHNUM); //NOTE ON & Channel Num
        conn.write(pitch); //Pitch
        conn.write(127); // Velocity
      }
    }
 
    // NOTE OFF
    if ((!(currtouched & _BV(i)) && (lasttouched & _BV(i))) ||  (airInBag == 0 && prevAirInBag != 0)) {
      if (DEBUG) { conn.print(i); conn.println(" released"); }
      else {
        //THESE ARE GETTING SENT BUT NO NOTE ON IF NO AIR
        conn.write(NOTE_OFF_CHNUM); // NOTE OFF
        conn.write(pitch);
        conn.write((byte)0);//PUTTING A ZERO HERE CAUSES AN ERROR
      }
    }
  }

  // reset our state
  lasttouched = currtouched; 
}



void senseSound () {  
  // read the value from the sensor:
  noises[cur_index] = analogRead(MIC_PIN);
  cur_index = (cur_index + 1) % buffer_size;
 // if (millis() - last_shift > 100){    //BEN - DEACTIVATED AS A DELAY IS SET IN THE LOOP
     //last_shift = millis();
     unsigned int avg = average(noises, buffer_size);
     unsigned long mic = variance(noises, buffer_size);
     mic_max = max(mic, mic_max);
     mic_min = min(mic, mic_min);
     MIDI_airVal = map(mic, mic_min, mic_max, 0, 127); //then map the input to 0-127 - your CC MIDI val
  //}
  
  //MIDI
  prevAirInBag = airInBag;
  int diff = abs(MIDI_airVal - last_MIDI_airVal);
  if (MIDI_airVal != last_MIDI_airVal && diff > MIC_TOLERANCE) {
    if (DEBUG) {
      conn.print("Volume:");
      conn.print(mic);
      conn.print("   ~    Raw input:");
      conn.print(noises[cur_index]);
      conn.print(", MIDI_airVal :"); conn.println(MIDI_airVal);
      
      //Print a row of xs as a volume meter
      String c = "x";
      for (int i=0; i<MIDI_airVal; i++) {
        c += "x";
      }
      conn.println (c);
    } else {
      airInBag += MIDI_airVal;
      if (airInBag>127) {airInBag = 127;}
    }
    
    last_MIDI_airVal = MIDI_airVal;
  }
  
  if (airInBag>0) {
    if (prevAirInBag == 0 && play_drone_on_blow) {
      if(DEBUG){ conn.println("NEW AIR! NOTE ON"); }
      else {
        conn.write(NOTE_ON_CHNUM);
        conn.write(DRONE_KEY);
        conn.write(127);
      }
    }
    
    if (DEBUG) { conn.print("Air left in bag :"); conn.println(airInBag); }
    else {
      conn.write(CC_CHNUM); //CC & Channel Num
      conn.write(MIC_CONTROLLER_NUM); //Controller Num
      int airMIDIVal = (int)airInBag;
      conn.write(airInBag); // Value
    }
    
    airInBag -= FALL_RATE; //decrease gradually
    if (airInBag<=0) {
      if (prevAirInBag > 0) {
        if(DEBUG) {
           conn.println("OUT OF AIR! NOTE OFF");
        } else {
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




int raw1 = 0;
int raw2 = 0;

int r1 = 0;
int r2 = 0;
int r3 = 0;
int r4 = 0;

int oldR1 = -1;
int oldR2 = -1;
int oldR3 = -1;
int oldR4 = -1;

int rArr[] = {r1,r2,r3,r4};
int oldRArr[] = {oldR1, oldR2, oldR3, oldR4};
unsigned char contArr[] = {REED_CONTROLLER_NUM1, REED_CONTROLLER_NUM2, REED_CONTROLLER_NUM3, REED_CONTROLLER_NUM4};
int noteArr[] = {REED_NO_NOFF_NUM1, REED_NO_NOFF_NUM2, REED_NO_NOFF_NUM3, REED_NO_NOFF_NUM4};


void senseReeds () {

  //UPDATE r vars
  raw1 = 1 - digitalRead(REED1_PIN); //PULLED HIGH BY PULLUP RESISTOR SO 1- BIT IS TO NORMALISE (1 = on)
  raw2 = 1 - digitalRead(REED2_PIN);

  if (DEBUG) {
      conn.print("RAW reed read. r1:");
      conn.print(r1);
      conn.print(", r2:");
      conn.println(r2);
  }
      
       if (raw1 == 1 && raw2 == 0) {r1 = 1; r2 = 0; r3 = 0; r4 = 0;} //1
  else if (raw1 == 0 && raw2 == 1) {r1 = 0; r2 = 1; r3 = 0; r4 = 0;} //2
  else if (raw1 == 1 && raw2 == 1) {r1 = 0; r2 = 0; r3 = 1; r4 = 0;} //both
  else if (raw1 == 0 && raw2 == 0) {r1 = 0; r2 = 0; r3 = 0; r4 = 1;} //none
  
  rArr[0]=r1;
  rArr[1]=r2;
  rArr[2]=r3;
  rArr[3]=r4;
  
  //Cycle through
  int n;
  for (n=1; n<=4 ;n++) {
    int i = n-1;
    if (rArr[i] != oldRArr[i]) {
      if (DEBUG) {
          conn.print("r1:");
          conn.print(rArr[0]);
          conn.print("(");conn.print(oldRArr[0]);
          conn.print(")\tr2:");
          conn.print(rArr[1]);conn.print("(");conn.print(oldRArr[1]);
          conn.print(")\tr3:");
          conn.print(rArr[2]);conn.print("(");conn.print(oldRArr[2]);
          conn.print(")\tr4:");
          conn.print(rArr[3]);conn.print("(");conn.print(oldRArr[3]);
          conn.println(")");
      } else {
          //
          if (reedsInCCMode) {
            //CC
            conn.write(CC_CHNUM);
            conn.write(contArr[i]);
            conn.write((rArr[i] * 126) + 1);  //SHOULD SET THE ACTIVE r TO 127 AND THE REST TO 1 (BUG WITH SETTING IT TO ZERO I THINK)
          } else {
            //NOTE ON/OFF
            if (rArr[i] == 1 && oldRArr[i] == 0) {
             conn.write(NOTE_ON_CHNUM); // NOTE ON
             conn.write(noteArr[i]);
             conn.write(127);
          } else if (rArr[i] == 0 && oldRArr[i] == 1) {
              conn.write(NOTE_OFF_CHNUM); // NOTE OFF
              conn.write(noteArr[i]);
              conn.write((byte)0); //PUTTING A ZERO HERES CAUSES AN ERROR
            }            
          }
      }
       oldRArr[0] = r1;
       oldRArr[1] = r2;
       oldRArr[2] = r3;
       oldRArr[3] = r4;
    }
  }
}









  
void senseTilt() {
  /* Get a new sensor event */ 
  sensors_event_t event; 
  accel.getEvent(&event);
  
  //These seem to be a max of +/-90 degrees

  float pitch = atan(event.acceleration.y/sqrt(pow(event.acceleration.x,2) + pow(event.acceleration.z,2)));
  float roll = atan(event.acceleration.z/sqrt(pow(event.acceleration.y,2) + pow(event.acceleration.z,2)));
  
  //convert radians into degrees
  pitch = pitch * (180.0/PI);
  roll = roll * (180.0/PI) ;

  pitch_midi_val = map(pitch, -90, 90, 0, 127);
  roll_midi_val = map(roll, -90, 90, 0, 127);
  
  /* Display the results (acceleration is measured in m/s^2) */
  //conn.print("X: "); conn.print(event.acceleration.x); conn.print("  ");
  //conn.print("Y: "); conn.print(event.acceleration.y); conn.print("  ");
  //conn.print("Z: "); conn.print(event.acceleration.z); conn.print("  "); conn.println("m/s^2 ");
 
  //PITCH
  if (pitch_active && (pitch_midi_val != last_pitch_midi_val) && abs(pitch_midi_val - last_pitch_midi_val) > pitch_tolerance) {
    //CONTROL CHANGE DATA
    if (DEBUG) {
      conn.print("Pitch: "); conn.print(pitch);
      conn.print("\t\tMIDI: "); conn.println(pitch_midi_val);
    } else {
      conn.write(CC_CHNUM); //CC & Channel Num
      conn.write(PITCH_CONTROLLER_NUM); //Controller Num
      conn.write(pitch_midi_val); // Value
    }

    last_pitch_midi_val = pitch_midi_val;
  }
  
  //ROLL
  if (roll_active && (roll_midi_val != last_roll_midi_val) && abs(roll_midi_val - last_roll_midi_val) > roll_tolerance) {
    if (DEBUG) {
      conn.print("\tRoll: "); conn.print(roll);
      conn.print("\t\tMIDI: "); conn.print(roll_midi_val); conn.println("");
    } else {
      conn.write(CC_CHNUM); //CC & Channel Num
      conn.write(ROLL_CONTROLLER_NUM); //Controller Num
      conn.write(roll_midi_val); // Value
    }
    last_roll_midi_val = roll_midi_val;
  }

}

void loop () {
  //if (conn.available()) //DONT USE THIS - DOESNT SEEM TO WORK
  //{
    senseSound();
  
    senseTouch();
    
    senseTilt();

    senseReeds();
  //}
  delay (50);
}
