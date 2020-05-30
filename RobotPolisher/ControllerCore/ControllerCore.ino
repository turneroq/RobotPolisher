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

//---------------- PING SETTINGS ----------------
#define TIMEOUT_THRESHOLD             200000 // 200ms in microsecs
#define BEST_NETWORK_TIME_THRESHOLD   40000 // 40ms in microsecs
#define MEDIUM_NETWORK_TIME_THRESHOLD 100000 // 100ms in microsecs
//---------------- PING SETTINGS ----------------


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
UTFT screen(TFT01_22SP, PIN_MOSI, PIN_SCK, PIN_CS, PIN_RESET, PIN_DC); 
extern uint8_t BigFont[]; // external fonts
//---------------- SCREEN SETTINGS ----------------


////---------------- RF CONFIG ----------------
RF24 radio(PIN_CE, PIN_CSN);
byte pipe_addresses[][6] = {"1Node","2Node"}; // first - robot, second - remote controller 
typedef enum { role_rc_tx = 1} role_en; // for future scaling
role_en role = role_rc_tx;
////---------------- RF CONFIG ----------------


typedef enum {no_cmd, forward = 1, on = 1, backward, off = 2, stop, best, medium, bad, timeout} commands;
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
  uint32_t last_time_sender_timer;
  uint32_t ping_initial_time;
} timer;

struct _cache
{
  int16_t last_cached_value;
  uint16_t first_visit;
  uint8_t check_ping;
  uint8_t network_drawing_flag;
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
volatile uint8_t state_auto_control = 0;
volatile uint8_t state_polish_control = 0;


void setup()
{
#if DEBUG_SERIAL
  Serial.begin(SERIAL_RATE);
#endif
  display_setup();
  radio_setup();  
  
  //initialize structs values
  coords.x = 0;
  coords.y = 0;
  prev_coords.x = 0;
  prev_coords.y = 0;

  uint32_t t = millis();
  timer.last_time_input_timer = t;
  timer.last_time_draw_timer = t;
  timer.last_time_battery_timer = t;
  timer.last_time_sender_timer = t;
  timer.ping_initial_time = 0;

  cache.last_cached_value = -1;
  cache.first_visit = 1;//must be true
  cache.check_ping = 0;
  cache.network_drawing_flag = 0;

  commands_cache.left_wheel_cmd = 0;
  commands_cache.right_wheel_cmd = 0;
  commands_cache.auto_mode_cmd = 0;
  commands_cache.front_motor_polisher_cmd = 0;
  commands_cache.ping_cmd = 0;
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
  const uint8_t success_packet = 254;
  radio.startListening();//enable receiver mode 
  radio.writeAckPayload(1, &success_packet, sizeof(success_packet)); //preload ACK to include it in response message
}


void display_setup()
{
  screen.InitLCD(ORIENTATION);
  screen.clrScr();
  screen.setFont(BigFont);
}


uint8_t radio_send(uint8_t* data, uint8_t bytes)
{
  radio.stopListening();
  uint8_t done = radio.write(data, bytes);
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
    return 0;
  }  
  return 1;
}


uint8_t feedback_handle(byte pipe_num)
{
  if(!radio.available(&pipe_num))//empty ack
  {
    //Assert that ACK MUSTN'T BE EMPTY!
    #if DEBUG_SERIAL
    Serial.println("ACK EMPTY ERROR");// but truly it isn't error. I wanna that ack size > 0
    #endif
    return 0;
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
    uint8_t code_ACK = 0;
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
      return 0;
    }
  }
  return 1;  
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
    commands_cache.left_wheel_cmd = forward;
    commands_cache.right_wheel_cmd = forward;
  }
  else if (coords.x < X_BACK_THRESHOLD)
  {
    commands_cache.left_wheel_cmd = backward;
    commands_cache.right_wheel_cmd = backward;
  }
  else if (coords.y < Y_LEFT_THRESHOLD)
  {
    commands_cache.left_wheel_cmd = stop;
    commands_cache.right_wheel_cmd = forward;
  }
  else if (coords.y > Y_RIGHT_THRESHOLD)
  {
    commands_cache.left_wheel_cmd = forward;
    commands_cache.right_wheel_cmd = stop;
  }
  else if(( X_BACK_THRESHOLD < coords.x && coords.x < X_FORWARD_THRESHOLD ) && ( Y_LEFT_THRESHOLD < coords.y && coords.y < Y_RIGHT_THRESHOLD))
  {
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
  commands_cache.auto_mode_cmd = state_auto_control;
  commands_cache.front_motor_polisher_cmd = state_polish_control;
}


void ping_checker()
{
  if(cache.check_ping)
  {
    commands_cache.ping_cmd = 1;
  }
}


uint8_t coord_changed()
{
  if(abs(prev_coords.x - coords.x) > NOISE_THRESHOLD)// X COORDS CHANGED
  {
    return 1;
  }
  else if(abs(prev_coords.y - coords.y) > NOISE_THRESHOLD)// Y COORDS CHANGED
  {
    return 1;
  }
  return 0;
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


int16_t battery_checker()
{
  /*
    1)Using voltage divider and get aprox 5V from new 9V battery
    2)Eventually voltage will drop and value on PIN_BATTERY_CHECK will decrease
  */
  if(timer_battery(10000) || cache.first_visit)
  {
    cache.first_visit = 0;
    int16_t current_value = analogRead(PIN_BATTERY_CHECK);//get data from check pin
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
  if(commands_cache.ping_cmd)
  {
    timer.ping_initial_time = micros();
  }
  radio_send(word, sizeof(uint8_t) * COMMUNICATION_WORD_SIZE);
}


void pong_response_check()
{
  if(commands_cache.ping_cmd) // ping was sent
  {
    uint8_t timeout_ = 0;
    //wait here for pong
    while(!radio.available())
    {
      if(micros() - timer.ping_initial_time > TIMEOUT_THRESHOLD)
      {
        timeout_ = 1;
        break;
      }
    }
    if(timeout_)
    {
      //TODO CHANGE STATUS FOR DRAWING BAD NETWORK CONNECTION
      cache.network_drawing_flag = timeout;
    }
    else
    {
      //TODO CHANGE STATUS FOR DRAWING QUALITY NETWORK CONNECTION
      uint32_t response_time = micros() - timer.ping_initial_time;
      if(response_time <= BEST_NETWORK_TIME_THRESHOLD)
      {
        cache.network_drawing_flag = best;
      }
      else if(response_time > BEST_NETWORK_TIME_THRESHOLD && response_time <= MEDIUM_NETWORK_TIME_THRESHOLD)
      {
        cache.network_drawing_flag = medium;
      }
      else if(response_time > MEDIUM_NETWORK_TIME_THRESHOLD && response_time < TIMEOUT_THRESHOLD)
      {
        cache.network_drawing_flag = bad;
      }
    }
  }
}


void loop()
{
  if(timer_input(50))
  {
    /*
      1)Get input
      2)Sets cmds for wheels or sets ping_cmd
      3)Check button status and sets auto + front_polish_cmd
      4)Sets ping cmd if coords(input) doesn't change
    */
    get_input();//get input from analog ports JOYSTICKS
    wheels_cmd_setter();
    buttons_checker();
    ping_checker();
  } 
  if(timer_sender(100))
  {
    /*
      1)Create communication word from commands cache
      2)Send word
      3)Wait if pong came
    */
    compose_send_commands_word();
    pong_response_check();
  }
  if(timer_draw(1000))
  {
    /*
      1)Drawing to screen
    */
    Serial.println("DREW");
    draw();
  }
}
