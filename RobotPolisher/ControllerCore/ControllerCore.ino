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



#define PIN_JOY_X A8
#define PIN_JOY_Y A9

#define X_FORWARD_THRESHOLD 800
#define X_BACK_THRESHOLD 200
#define Y_LEFT_THRESHOLD 200
#define Y_RIGHT_THRESHOLD 800
#define NOISE_THRESHOLD 150

#define DEBUG_SERIAL 1
#define SERIAL_RATE 115200
#define COMMUNICATION_WORD_SIZE 5

#define TIMEOUT_THRESHOLD 200 // microseconds

//---------------- RF SETTINGS ----------------
#define PIN_CE  7
#define PIN_CSN 6
//---------------- RF SETTINGS ----------------


//---------------- BATTERY_CHECKER SETTINGS ----------------
#define PIN_BATTERY_CHECK A0
#define THRESHOLD_UPPER   800 //CHECK REAL VALUES
#define THRESHOLD_MIDDLE  700 //CHECK REAL VALUES
#define THRESHOLD_LOWER   600 //CHECK REAL VALUES
//---------------- BATTERY_CHECKER SETTINGS ----------------

//---------------- BUTTONS SETTINGS ----------------
#define PIN_BUTTON_SWITCH_POLISH        21
#define PIN_BUTTON_SWITCH_AUTO_CONTROL  20
//---------------- BUTTONS SETTINGS ----------------

//---------------- SCREEN SETTINGS ----------------
#define PIN_CS    13
#define PIN_RESET 12
#define PIN_DC    11
#define PIN_MOSI  10
#define PIN_SCK   9
#define ORIENTATION LANDSCAPE // or PORTRAIT
//---------------- SCREEN SETTINGS ----------------


UTFT screen(TFT01_22SP, PIN_MOSI, PIN_SCK, PIN_CS, PIN_RESET, PIN_DC); 

extern uint8_t BigFont[]; // external fonts

RF24 radio(PIN_CE, PIN_CSN);
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

typedef enum {no_cmd, forward = 1, on = 1, backward, off = 2, stop} commands;
commands comms = forward;

struct robot_coords
{
  uint16_t x;
  uint16_t y;
} coords, prev_coords;

struct _timer
{
  uint32_t last_time_input_timer;
  uint32_t last_time_draw_timer;
  uint32_t last_time_battery_timer;
  uint32_t last_time_sender_timer
} timer;

const byte success_packet = 2;

struct _cache
{
  int32_t last_cached_value;
  uint16_t first_visit;
  uint8_t check_ping;//
  uint8_t _alignment;//
} cache;

struct _commands_cache
{
  uint8_t left_wheel_cmd;
  uint8_t right_wheel_cmd;
  uint8_t auto_mode_cmd;
  uint8_t front_motor_polisher_cmd;
  uint8_t ping_cmd;
  uint8_t _alignment_a;
  uint8_t _alignment_b;
  uint8_t _alignment_c;
} commands_cache;

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
  
  pinMode(PIN_BATTERY_CHECK, INPUT);

//initialize structs values
  timer.last_time_input_timer = millis();
  timer.last_time_draw_timer = millis();
  timer.last_time_battery_timer = millis();
  timer.last_time_sender_timer = millis();

  cache.last_cached_value = -1;
  cache.first_visit = 1;//must be true
  cache.check_ping = 0;
  cache._alignment = 0;

  coords.x = 0;
  coords.y = 0;
  prev_coords.x = 0;
  prev_coords.y = 0;

  commands_cache.left_wheel_cmd = 0;
  commands_cache.right_wheel_cmd = 0;
  commands_cache.auto_mode_cmd = 0;
  commands_cache.front_motor_polisher_cmd = 0;
  commands_cache.pind_cmd = 0;
  commands_cache._alignment_a = 0;
  commands_cache._alignment_b = 0;
  commands_cache._alignment_c = 0;
  
  //TURN(ON, OFF) auto control
  attachInterrupt(digitalPinToInterrupt(PIN_BUTTON_SWITCH_AUTO_CONTROL), set_robot_auto_control, FALLING  );
  //TURN(ON, OFF) polish
  attachInterrupt(digitalPinToInterrupt(PIN_BUTTON_SWITCH_POLISH), set_robot_polish_control, FALLING  );
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


bool radio_send(char* data, uint64_t len)
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


void get_input()
{
  //save previous values
  prev_coords = coords;

  //getting new values
  coords.x = analogRead(PIN_JOY_X);
  coords.y = analogRead(PIN_JOY_Y); 
}


void wheels_cmd_setter()
{
  //Func sets commands for robot's wheels

  //!!! JOY position: wires down  
  #if DEBUG_SERIAL
    if(coord_changed())
    {
      Serial.print("X = ");
      Serial.println(coords.x);
      Serial.print("Y = ");
      Serial.println(coords.y);
    }
  #endif
  
  if(!coord_changed())
  {
    //Serial.println("COORDS NOT CHANGED");
    //Nothing changed. Or noise in analog ports
    cache.check_ping = on;
    return;
  }
  else if (coords.x > X_FORWARD_THRESHOLD)
  {
    //both wheels go ahead
    //radio_send(10 * forward + forward, sizeof(forward));
    commands_cache.left_wheel_cmd = forward;
    commands_cache.right_wheel_cmd = forward;
  }
  else if (coords.x < X_BACK_THRESHOLD)
  {
    //bothe wheels go back
    //radio_send(10 * back + back, sizeof(back));
    commands_cache.left_wheel_cmd = backward;
    commands_cache.right_wheel_cmd = backward;
  }
  else if (coords.y < Y_LEFT_THRESHOLD)
  {
    //forward + left = turn left
    //radio_send(10 * stop + forward, sizeof(forward));
    commands_cache.left_wheel_cmd = stop;
    commands_cache.right_wheel_cmd = forward;
  }
  else if (coords.y > Y_RIGHT_THRESHOLD)
  {
    //forward + right = turn right
    //radio_send(10 * forward + stop, sizeof(forward));
    commands_cache.left_wheel_cmd = forward;
    commands_cache.right_wheel_cmd = stop;
  }
  else if(( X_BACK_THRESHOLD < coords.x && coords.x < X_FORWARD_THRESHOLD ) && ( Y_LEFT_THRESHOLD < coords.y && coords.y < Y_RIGHT_THRESHOLD))
  {
    //stop
    //radio_send(10 * stop + stop, sizeof(stop));
    commands_cache.left_wheel_cmd = stop;
    commands_cache.right_wheel_cmd = stop;
  }
  cache.check_ping = no_cmd;
}


void buttons_checker()
{
  //TODO IF CORRESPONDING BUTTON WAS PRESSED 
  //commands_cache.auto_mode_cmd = STATUS_AUTO_MODE_BUTTON
  //commands_cache.front_motor_polisher_cmd = STATUS_FRONT_MOTOR_POLISHER_BUTTON
}


void ping_checker()
{
  if(cache.check_ping)
  {
    commands_cache.ping_cmd = 1;
  }
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


//input checker timer 
uint8_t timer_input(uint32_t msec, uint32_t last_time_passed = timer.last_time_input_timer)
{
  if(millis() - last_time_passed > msec)
  {
    timer.last_time_input_timer = millis();
    return 1;
  }
  return 0;
}


//drawing routine timer
uint8_t timer_draw(uint32_t msec, uint32_t last_time_passed = timer.last_time_draw_timer)
{
  if(millis() - last_time_passed > msec)
  {
    timer.last_time_draw_timer = millis();
    return 1;
  }
  return 0;
}


//sender routine timer
uint8_t timer_sender(uint32_t msec, uint32_t last_time_passed = timer.last_time_sender_timer)
{
  if(millis() - last_time_passed > msec)
  {
    timer.last_time_sender_timer = millis();
    return 1;
  }
  return 0;
}


//battery checker timer
uint8_t timer_battery(uint32_t msec, uint32_t last_time_passed = timer.last_time_battery_timer)
{
  if(millis() - last_time_passed > msec)
  {
    timer.last_time_battery_timer = millis();
    return 1;
  }
  return 0;
}


#include "RenderLib.h"


int32_t battery_checker()
{
  if(timer_battery(10000) || cache.first_visit)
  {
    cache.first_visit = 0;
    int32_t current_value = analogRead(PIN_BATTERY_CHECK);//get data from check pin
    cache.last_cached_value = current_value;
    if(!current_value)
    {
      //Error
      //In most cases the circuit voltage divider is opened
      #if DEBUG_SERIAL
      Serial.println("ERROR IN BATTERY CHECKER. CHECK CONNECTION WITH PIN_BATTERY_CHECK");
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



void compose_send_commands_word()
{
  /*
    array of some values[
      LEFT_WHEEL_CMD,           CMDs {0:NO_CMD, 1:FORWARD, 2:BACKWARD, 3:STOP}
      RIGHT_WHEEL_CMD,          CMDs {0:NO_CMD, 1:FORWARD, 2:BACKWARD, 3:STOP}
      AUTO_MOD_WHEELS_CMD,      CMDs {0:NO_CMD, 1:ON, 2:OFF}
      FRONT_MOTOR_POLISHER_CMD  CMDs {0:NO_CMD, 1:ON, 2:OFF}
      PING_CMD                  CMDs {0:NO_CMD, 1:SEND_FEEDBACK}
    ]
  */
  uint8_t word[COMMUNICATION_WORD_SIZE] = {
    commands_cache.left_wheel_cmd,
    commands_cache.right_wheel_cmd,
    commands_cache.auto_mode_cmd,
    commands_cache.front_motor_polisher_cmd,
    commands_cache.ping_cmd
  };
  radio_send(&word, sizeof(uint8_t) * COMMUNICATION_WORD_SIZE);
}


void loop()
{
  if(timer_input(50))
  {
    get_input();//get input from analog ports JOYSTICKS
    wheels_cmd_setter();
    buttons_checker();
    ping_checker();
  } 
  if(timer_sender(100))
  {
    compose_send_commands_word();
  }
  if(timer_draw(1000))
  {
    Serial.println("DREW");
    draw();
  }
}