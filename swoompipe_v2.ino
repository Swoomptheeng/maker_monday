/* 12x Capacitive Keys, a Mic and a gyroscope sending MIDI over Bluetooth */
/* NOTE : havn't actually tested this code (as it's very slightly differn't from that in use - I removed lots of comments and debris - I assume it all still works!) */

#include <Wire.h>
#include "Adafruit_MPR121.h"
#include <SoftwareSerial.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>

/* Assign a unique ID to this sensor at the same time */
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

Adafruit_MPR121 cap = Adafruit_MPR121();


//TOUCH
uint16_t lasttouched = 0;
uint16_t currtouched = 0;

//MIDI
static boolean sendMIDI = true;
int MIDINoteArr[] = {54,55,56,57,58,59,60,61,62,63,64,65};

//BLUETOOTH
SoftwareSerial conn(10, 11); // RX, TX

void senseTouch()
{
  //CAPACITATIVE TOUCH
  // Get the currently touched pads
  currtouched = cap.touched();
  
  for (uint8_t i=0; i<12; i++) 
  {
    int pitch = MIDINoteArr[i];
 
    // NOTE ON
    if ((currtouched & _BV(i)) && !(lasttouched & _BV(i)) ) 
    {
      //MIDI - NOTE ON
      if (sendMIDI)
      {
        conn.write(0x95); //NOTE ON & Channel Num
        conn.write(pitch); //Pitch
        conn.write(127); // Velocity
      }
      else
      {
        conn.print(i); conn.println(" touched");
      }
    }
    
    // NOTE OFF
    if (!(currtouched & _BV(i)) && (lasttouched & _BV(i)) ) 
    {
      if (sendMIDI)
      {
        conn.write(0x85); // NOTE OFF
        conn.write(pitch);
        conn.write(1);//THIS WAS SET TO 0 BUT THIS CAUSES  ERROR - SHOULDN'T MATTER THOUGH AS IT'S A NOTE OFF MESSAGE BEING SENT
      }
      else
      {
        conn.print(i); conn.println(" released");
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
int MIC_PIN = A0;
//unsigned long last_shift = 0;
unsigned long mic_max = 0;
unsigned long mic_min = 99999999;
static int MIC_TOLERANCE = 1;

//MIDI
int last_MIDI_val = 0;
int MIDI_val = 1;

void senseSound ()
{  
  // read the value from the sensor:
	noises[cur_index] = analogRead(MIC_PIN);
	cur_index = (cur_index + 1) % buffer_size;
	unsigned int avg = average(noises, buffer_size);
	unsigned long mic = variance(noises, buffer_size);
	mic_max = max(mic, mic_max);
	mic_min = min(mic, mic_min);
	MIDI_val = map(mic, mic_min, mic_max, 0, 127); //then map the input to 0-127 - your CC MIDI val

  //MIDI
  int diff = abs(MIDI_val - last_MIDI_val);
  if (MIDI_val != last_MIDI_val && diff > MIC_TOLERANCE)
  {
    if (sendMIDI)
    {
      conn.write(0xB5); //CC & Channel Num
      conn.write(0x02); //Controller Num
      conn.write(MIDI_val); // Value
    }
    else
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
    
    last_MIDI_val = MIDI_val;
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


void setup () 
{

  conn.begin (9600);
  
  if (!sendMIDI)
  {
    conn.println("Started bluetooth connection");
  }
    
  if (!cap.begin(0x5A)) {
    conn.println("MPR121 not found, check wiring?");
    while (1);
  }
  
  if(!accel.begin())
  {
    conn.println("Ooops, no ADXL345 detected ... Check your wiring!");
    while(1);
  }
  accel.setRange(ADXL345_RANGE_16_G);
  sensor_t sensor;
  accel.getSensor(&sensor);
  
  for (int i = 0 ; i < buffer_size; i++) noises[i] = 512;//init noises array
   
  pinMode(FLEX_PIN, INPUT);
  pinMode(MIC_PIN, INPUT);
  
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
  int pitch = 180 * atan (event.acceleration.x/sqrt(event.acceleration.y*event.acceleration.y + event.acceleration.z*event.acceleration.z))/M_PI;
  int roll = 180 * atan (event.acceleration.y/sqrt(event.acceleration.x*event.acceleration.x + event.acceleration.z*event.acceleration.z))/M_PI;
  pitch_midi_val = map(pitch, -90, 90, 0, 127);
  roll_midi_val = map(roll, -90, 90, 0, 127);
  
  int pitch_tolerance = 2;
  int roll_tolerance = 2;
  
  if (sendMIDI)
  {
    if (pitch_midi_val != last_pitch_midi_val && abs(pitch_midi_val - last_pitch_midi_val) > pitch_tolerance)
    {
      conn.write(0xB5); //CC & Channel Num
      conn.write(0x03); //Controller Num
      conn.write(pitch_midi_val); // Value
      last_pitch_midi_val = pitch_midi_val;
    }
    
    if (roll_midi_val != last_roll_midi_val && abs(roll_midi_val - last_roll_midi_val) > roll_tolerance)
    {
      conn.write(0xB5); //CC & Channel Num
      conn.write(0x04); //Controller Num
      conn.write(roll_midi_val); // Value
      last_roll_midi_val = roll_midi_val;
    }
  }
  else
  {
    conn.print("Pitch: "); conn.print(pitch); conn.print("  ");
    conn.print("MIDI: "); conn.print(pitch_midi_val); conn.print("  ");
    conn.print("Roll: "); conn.print(roll); conn.print(" ");
    conn.print("MIDI: "); conn.print(roll_midi_val); conn.println("");
  }
}

void loop () 
{
    senseSound();
  
    senseTouch();

    senseTilt();

    delay (50);
}
