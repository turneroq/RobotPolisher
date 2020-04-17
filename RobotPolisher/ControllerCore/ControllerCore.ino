/*
  2/1/2020
  Remote controller for robot cleaner.

  Libs for:
    -NRF24L01
    -DISPLAY TFT 240*320
    -JOYSTICKS
    -BUTTONS
*/

#include <nRF24L01.h>
#include <printf.h>
#include <RF24.h>
#include <RF24_config.h>
#include <memorysaver.h>
#include <UTFT.h>



#define JOY_X A8
#define JOY_Y A9

#define X_FORWARD_THRESHOLD 800
#define X_BACK_THRESHOLD 200
#define Y_LEFT_THRESHOLD 200
#define Y_RIGHT_THRESHOLD 800
#define NOISE_THRESHOLD 150

#define DEBUG_SERIAL 1
#define SERIAL_RATE 115200

#define TIMEOUT_THRESHOLD 200 // microseconds

//---------------- RF SETTINGS ----------------
#define CE  7
#define CSN 6
//---------------- RF SETTINGS ----------------


//---------------- BATTERY_CHECKER SETTINGS ----------------
#define BATTERY_CHECK_PIN A0
#define THRESHOLD_UPPER   800 //CHECK REAL VALUES
#define THRESHOLD_MIDDLE  700 //CHECK REAL VALUES
#define THRESHOLD_LOWER   600 //CHECK REAL VALUES
//---------------- BATTERY_CHECKER SETTINGS ----------------

//---------------- BUTTONS SETTINGS ----------------
#define BUTTON_SWITCH_POLISH        21
#define BUTTON_SWITCH_AUTO_CONTROL  20
//---------------- BUTTONS SETTINGS ----------------

//---------------- SCREEN SETTINGS ----------------
#define CS    13
#define RESET 12
#define DC    11
#define MOSI  10
#define SCK   9
#define ORIENTATION LANDSCAPE // or PORTRAIT
//---------------- SCREEN SETTINGS ----------------


UTFT screen(TFT01_22SP, MOSI, SCK, CS, RESET, DC); 

extern uint8_t BigFont[]; // external fonts

RF24 radio(CE, CSN);
////---------------- RF CONFIG ----------------
byte pipe_addresses[][6] = {"1Node","2Node"}; // first - robot, second - remote controller 
typedef enum { role_rc_tx = 1} role_en; // for future scaling
role_en role = role_rc_tx;
////---------------- RF CONFIG ----------------

const char* str_comms[4] = {
  "forward",
  "back",
  "left",
  "right"
};//commands for robot

typedef enum {forward = 1, back, left, right, stop, ping} commands;
commands comms = forward;

struct robot_coords
{
  int x;
  int y;
} coords, prev_coords;

struct _timer
{
  unsigned long last_time_input_timer;
  unsigned long last_time_draw_timer;
  unsigned long last_time_battery_timer;
} timer;

const byte success_packet = 2;

struct _cache
{
  int last_cached_value;
  int first_visit;//int used instead bool for memory alignment
} cache;

//Flags used by interrupt routines
volatile int state_auto_control = LOW;
volatile int state_polish_control = LOW;


void setup()
{
  
#if DEBUG_SERIAL
  Serial.begin(SERIAL_RATE);
#endif
  display_setup();
  radio_setup();
  
  pinMode(BATTERY_CHECK_PIN, INPUT);

//initialize structs values
  timer.last_time_input_timer = millis();
  timer.last_time_draw_timer = millis();

  cache.last_cached_value = -1;
  cache.first_visit = true;//must be true

  coords.x = 0;
  coords.y = 0;
  prev_coords.x = 0;
  prev_coords.y = 0;

  //TURN(ON, OFF) auto control
  attachInterrupt(digitalPinToInterrupt(BUTTON_SWITCH_AUTO_CONTROL), set_robot_auto_control, FALLING  );
  //TURN(ON, OFF) polish
  attachInterrupt(digitalPinToInterrupt(BUTTON_SWITCH_POLISH), set_robot_polish_control, FALLING  );
}


void radio_setup()
{
  radio.begin();

  radio.enableAckPayload();
  //radio.setDataRate(RF24_250KBPS);
  if(role == role_rc_tx)
  {
    //robot is data receiver from console but rarely would like to send data
    radio.openWritingPipe(pipe_addresses[0]);//pipe address to write
    radio.openReadingPipe(1, pipe_addresses[1]);//pipe num and address to read
  }
  radio.startListening();//enable receiver mode 
  radio.writeAckPayload(1, &success_packet, sizeof(success_packet)); //preload ACK to include it in response message
}


void display_setup()
{
  screen.InitLCD(ORIENTATION);
  screen.clrScr();
  screen.setFont(BigFont);
}


bool radio_send(byte data, uint64_t len)
{
  #if DEBUG_SERIAL
  Serial.print("SENDING: ");
  Serial.println(data);
  #endif
  radio.stopListening();
  //uint64_t cur_time = micros();
  bool done = radio.write(&data, len);
  if(done)
  {
    //the message is successfully acknowledged by the receiver OR the timeout/retransmit maxima weren't reached
    if(!feedback_handle(1))
    {
      #if DEBUG_SERIAL
      Serial.println("SENDING ACK WARNING");
      #endif
    }
  }
  else
  {
    #if DEBUG_SERIAL
    //message isn't successfully acknowledged by the receiver OR the timeout/retransmit maxima were reached
    Serial.println("SENDING TIMEOUT ERROR");
    #endif
    return false;
  }
  #if DEBUG_SERIAL
  Serial.print("SENT: ");
  Serial.println(data);
  #endif
  return true;
}


bool feedback_handle(byte pipe_num)
{
  if(!radio.available(&pipe_num))//empty ack
  {
    //Assert that ACK MUSTN'T BE EMPTY!
    #if DEBUG_SERIAL
    Serial.println("ACK EMPTY ERROR");// but truly it isn't error. I wanna that ack size > 0
    #endif
    return false;
  }
  else //non-empty ack
  {
    /*
    if(radio.getDynamicPayloadSize() < strlen(success_packet))
    {
      //Corrupt payload has been flushed
      Serial.println("SMALL PAYLOAD SIZE ERROR");
      Serial.print("DynamicPayloadSize = ");
      Serial.println(radio.getDynamicPayloadSize());
      return false;
    }
    */
    byte code_ACK = 0;
    radio.read(&code_ACK, 1);
    if(code_ACK == 2)
    {
      #if DEBUG_SERIAL
      Serial.println("ACK: 2");
      #endif
    }
    else
    {
      #if DEBUG_SERIAL
      Serial.println("ACK: TRASH");
      #endif
    }
    if(!validate_ack(code_ACK))
    {
      return false;
    }
  }
  return true;  
}


bool validate_ack(byte ack)
{
  if(ack != 2) return false;
  return true;
}


void get_input()
{
  //save previous values
  prev_coords = coords;

  //getting new values
  coords.x = analogRead(JOY_X);
  coords.y = analogRead(JOY_Y); 
}


//send commands corresponds coords of input
void handle_input()
{
  //send commands on every wheel
  //EXAMPLE forward both, wheels forward = 1, therefore 11
  //back both, wheels back = 2, therefore 22
  //left wheel back = 2, right forward = 1, therefore 21

  //!!! JOY position: wires down
  if(coord_changed())
  {
    #if DEBUG_SERIAL
    Serial.print("X = ");
    Serial.println(coords.x);
    Serial.print("Y = ");
    Serial.println(coords.y);
    #endif
  }
  
  if(!coord_changed())
  {
    //Serial.println("COORDS NOT CHANGED");
    //Nothing changed. Or noise in analog ports
    return;
  }
  else if (coords.x > X_FORWARD_THRESHOLD)
  {
    //both wheels go ahead
    radio_send(10 * forward + forward, sizeof(forward));
  }
  else if (coords.x < X_BACK_THRESHOLD)
  {
    //bothe wheels go back
    radio_send(10 * back + back, sizeof(back));
  }
  else if (coords.y < Y_LEFT_THRESHOLD)
  {
    //forward + left = turn left
    radio_send(10 * stop + forward, sizeof(forward));
  }
  else if (coords.y > Y_RIGHT_THRESHOLD)
  {
    //forward + right = turn right
    radio_send(10 * forward + stop, sizeof(forward));
  }
  else if(( X_BACK_THRESHOLD < coords.x && coords.x < X_FORWARD_THRESHOLD ) && ( Y_LEFT_THRESHOLD < coords.y && coords.y < Y_RIGHT_THRESHOLD))
  {
    //stop
    radio_send(10 * stop + stop, sizeof(stop));
  }
}


void handle_buttons()
{
  //
}

bool coord_changed()
{
  if(abs(prev_coords.x - coords.x) > NOISE_THRESHOLD)// X COORDS CHANGED
  {
    return true;
  }
  else if(abs(prev_coords.y - coords.y) > NOISE_THRESHOLD)// Y COORDS CHANGED
  {
    return false;
  }
  return false;
}

//interrupt routine
void set_robot_auto_control()
{
    state_auto_control = !state_auto_control;
}

//interrupt routine
void set_robot_polish_control()
{
    state_polish_control = !state_polish_control;
}


//timer
bool timer_input(unsigned long msec, unsigned long last_time_passed = timer.last_time_input_timer)
{
  if(millis() - last_time_passed > msec)
  {
    timer.last_time_input_timer = millis();
    return true;
  }
  return false;
}


bool timer_draw(unsigned long msec, unsigned long last_time_passed = timer.last_time_draw_timer)
{
  if(millis() - last_time_passed > msec)
  {
    timer.last_time_draw_timer = millis();
    return true;
  }
  return false;
}


bool timer_battery(unsigned long msec, unsigned long last_time_passed = timer.last_time_battery_timer)
{
  if(millis() - last_time_passed > msec)
  {
    timer.last_time_battery_timer = millis();
    return true;
  }
  return false;
}

#include "RenderLib.h"

int battery_checker()
{
  if(timer_battery(10000) || cache.first_visit)
  {
    cache.first_visit = 0;
    int current_value = analogRead(BATTERY_CHECK_PIN);//get data from check pin
    cache.last_cached_value = current_value;
    if(!current_value)
    {
      //Error
      //In most cases the circuit voltage divider is opened
      #if DEBUG_SERIAL
      Serial.println("ERROR IN BATTERY CHECKER. CHECK CONNECTION WITH BATTERY_CHECK_PIN");
      #endif
      return -1;
    }
    if(current_value >= THRESHOLD_UPPER)
    {
      return 3;
    }
    else if(THRESHOLD_MIDDLE <= current_value && current_value < THRESHOLD_UPPER)
    {
      return 2;
    }
    else if(THRESHOLD_LOWER <= current_value && current_value < THRESHOLD_MIDDLE)
    {
      return 1;
    }
    else
    {
      return 0;
    }
  }
  else
  {
    return cache.last_cached_value;
  }
}


void draw()
{
  header_properties_setup();//MUST BE FIRST
  draw_battery();
  draw_line_under_header();
  draw_network_columns();
  draw_time_elapsed();
  draw_flags();
}




void loop()
{
  
  if(timer_input(50))
  {
    get_input();//get input from analog ports JOYSTICKS
    handle_input();
  } 
  if(timer_draw(1000))
  {
    Serial.println("DRAWED");
    draw();
  }
}