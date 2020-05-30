/*
  1/6/2020
  Robot cleaner prototype code.

  Libs for:
    -NRF24L01
    -STEP DC 5V
    -DC 5V
*/

#include <CustomStepper.h>

#include <nRF24L01.h>
#include <printf.h>
#include <RF24.h>
#include <RF24_config.h>

#define DEBUG_SERIAL 1
#define SERIAL_RATE 115200

//---------------- STEP MOTOR PINS ----------------
#define MLeft_IN1 7
#define MLeft_IN2 6
#define MLeft_IN3 5
#define MLeft_IN4 4

#define MRight_IN1 8
#define MRight_IN2 9
#define MRight_IN3 10
#define MRight_IN4 11
//---------------- STEP MOTOR PINS ----------------

//---------------- STEP MOTOR SETTINGS ----------------
#define MLEFT_RPM   18
#define MLEFT_SPR   4000
#define MRIGHT_RPM  18
#define MRIGHT_SPR  4000
#define NUMBER_ROTATES 1
//---------------- STEP MOTOR SETTINGS ----------------

//---------------- DC MOTOR SETTINGS ----------------
#define ROT_EN12 22
#define ROT_IN1 2
#define ROT_IN2 3
//---------------- DC MOTOR SETTINGS ----------------

//---------------- RF SETTINGS ----------------
#define CSN 13
#define CE  12
#define CHECK_RADIO_THRESHOLD 500 //millisec
//---------------- RF SETTINGS ----------------
//MEGA HARDWARE MOSI 51, MISO 50, SCK 52


CustomStepper MLeft(MLeft_IN1, MLeft_IN2, MLeft_IN3, MLeft_IN4, (byte[]){8, B1000, B1100, B0100, B0110, B0010, B0011, B0001, B1001}, MLEFT_SPR, MLEFT_RPM, CW);
CustomStepper MRight(MRight_IN1, MRight_IN2, MRight_IN3, MRight_IN4);
RF24 radio(CE, CSN);
////---------------- RF CONFIG ----------------
byte pipe_addresses[][6] = {"1Node","2Node"}; //First - robot, second - console 
typedef enum { role_robot = 1} role_en; // for future scaling
role_en role = role_robot;
const byte success_packet = 2;
byte cur_cmd = 0;
unsigned long last_time_check = 0;
////---------------- RF CONFIG ----------------

struct motor_status
{
  byte left;
  byte right;
  byte speed;
  byte reserved;
} state, prev_state;


typedef enum {forward = 1, back, left, right, stop, ping} commands;
commands comms = forward;

unsigned long last_time_timer = 0;

void setup()
{
#if DEBUG_SERIAL
  Serial.begin(SERIAL_RATE);
#endif


  pinMode(ROT_EN12, OUTPUT);
  pinMode(ROT_IN1,  OUTPUT);
  pinMode(ROT_IN2,  OUTPUT);
  
  pinMode(MLeft_IN1, OUTPUT);
  pinMode(MLeft_IN2, OUTPUT);
  pinMode(MLeft_IN3, OUTPUT);
  pinMode(MLeft_IN4, OUTPUT);

  pinMode(MRight_IN1, OUTPUT);
  pinMode(MRight_IN2, OUTPUT);
  pinMode(MRight_IN3, OUTPUT);
  pinMode(MRight_IN4, OUTPUT);

  MLeft.setRPM(MLEFT_RPM); // RPM. Than bigger RPM is than power of motor is lower
  MLeft.setSPR(MLEFT_SPR); // Steps per FULL round of shaft

  MRight.setRPM(MRIGHT_RPM);
  MRight.setSPR(MRIGHT_SPR);

  state.left = stop;
  state.right = stop;
  state.speed = stop;

  prev_state.left = stop;
  prev_state.right = stop;
  prev_state.speed = stop;
  
  last_time_timer = millis();
  
  radio_setup();
}


void radio_setup()
{
  radio.begin();

  radio.enableAckPayload();
  //radio.setDataRate(RF24_250KBPS);
  
  if(role == role_robot)
  {
    //robot is data receiver from console but rarely would like to send data
    radio.openWritingPipe(pipe_addresses[1]);//pipe address to write
    radio.openReadingPipe(1, pipe_addresses[0]);//pipe num and address to read
  }
  radio.startListening();//enable receiver mode

  radio.writeAckPayload(1, &success_packet, sizeof(success_packet)); //preload ACK to include it in response message
}


void drive()
{
  if(state_changed())
  {
    #if DEBUG_SERIAL
    Serial.println("STATE_CHANGED");
    #endif
    if(MLeft.isDone())
    {
      switch(state.left)
      { 
        case forward:
        {
          MLeft.setDirection(CCW);
          MLeft.rotate(NUMBER_ROTATES);
          break;
        }
        case back:
        {
          MLeft.setDirection(CW);
          MLeft.rotate(NUMBER_ROTATES);
          break;
        }
        default:
        {
          MLeft.setDirection(STOP);
          //MLeft.setDirection(CW);
          //MLeft.rotate(NUMBER_ROTATES);
          break;
        }
      }
    }
    else
    {
      #if DEBUG_SERIAL
      //Serial.println("MLEFT NOT DONE");
      #endif
    }
    if(MRight.isDone())
    {
      switch(state.right)
      {
        case forward:
        {
          MRight.setDirection(CW);
          MRight.rotate(NUMBER_ROTATES);
          break;
        }
        case back:
        {
          MRight.setDirection(CCW);
          MRight.rotate(NUMBER_ROTATES);
          break;
        }
        default:
        {
          MRight.setDirection(STOP);
          //MRight.rotate(1);
          //MRight.setDirection(CW);
          //MRight.rotate(NUMBER_ROTATES);
          break;
        }
      }
    }
    else
    {
      //if(passed(300))
      //{
        #if DEBUG_SERIAL
        //Serial.println("MRIGHT NOT DONE"); 
        #endif
      //}
    } 
  }
}


bool state_changed()
{
  #if DEBUG_SERIAL
  Serial.print("prev_state.left = ");
  Serial.println(prev_state.left);
  Serial.print("state.left = ");
  Serial.println(state.left);
  Serial.print("prev_state.right = ");
  Serial.println(prev_state.right);
  Serial.print("state.right = ");
  Serial.println(state.right);
  #endif
  
  if(prev_state.left != state.left || prev_state.right != state.right)
  {
    return true;
  }
  return true;
}


//first - power, sec - direction
void rotator(byte pwm, bool d = false)
{
  digitalWrite(ROT_EN12, true);
  if(d)
  {
    analogWrite(ROT_IN1, pwm);
    analogWrite(ROT_IN2, 0); 
  }
  else
  {
    analogWrite(ROT_IN1, 0);
    analogWrite(ROT_IN2, pwm); 
  }
}


//checker with timer depends on CHECK_RADIO_THRESHOLD
bool radio_check(byte pipe_n)
{
  if(millis() - last_time_check > CHECK_RADIO_THRESHOLD)
  {
    last_time_check = millis();
    if(radio.available(&pipe_n))
    {
      return true;
    }
  }
  return false;
}


void radio_read(byte pipe_number)
{
    byte data = 0;//byte with commands from remote controller
    radio.read(&data, sizeof(data));//reading
    cur_cmd = data;//saving current valid commands for next funcs
    //data = temp;//DELETE WHEN NRF WORKS
    //cur_cmd = temp;
    
    //Translate digits mnemonics to names
    String command = "";
    switch(data)
    {
      case 11:
      {
        command = "forward";
        break;
      }
      case 22:
      {
        command = "back";
        break;
      }
      case 51:
      {
        command = "turn left";
        break;
      }
      case 15:
      {
        command = "turn right";
        break;
      }
      case 55:
      {
        command = "STOP";
        break;
      }
      case 6:
      {
        command = "ping";
        break;
      }
      default:
      {
        command = "UNRECOGNIZABLE";
        cur_cmd = 0;
        break;
      }      
    };
#if DEBUG_SERIAL
    Serial.print("ROBOT RECEIVED CMD: ");
    Serial.println(command);
    //Serial.print(" - size ");
    //Serial.println(sizeof(data));
    Serial.print("cur_cmd = ");
    Serial.println(cur_cmd);
#endif
    radio_send_feedback(pipe_number);//ACK
}


void radio_send_feedback(byte pipe_num)
{
  radio.writeAckPayload(pipe_num, &success_packet, sizeof(success_packet));
}


void handle_input()
{
  byte cmd_left_wheel = cur_cmd / 10; // 25 / 10 = 2
  byte cmd_right_wheel = cur_cmd % 10; // 25 % 10 = 5
  #if DEBUG_SERIAL
  //Serial.print("cmd_left_wheel = ");
  //Serial.println(cmd_left_wheel);
  //Serial.print("cmd_right_wheel = ");
  //Serial.println(cmd_right_wheel);
  #endif

  prev_state = state;
  
  if(!cur_cmd)
  {
    #if DEBUG_SERIAL
    Serial.println("Unrecognizable command cannot be handled");
    #endif
    state.left = stop;
    state.right = stop;
    return;
  }
  else if (cur_cmd == ping)
  {
    //TODO PING HANDLING CMD
  }
  
  //handle commands for left wheel
  switch(cmd_left_wheel)
  {
    case forward:
    {
      state.left = forward;
      break;
    }
    case back:
    {
      state.left = back;
      break;
    }
    case left:
    {
      break;
    }
    case right:
    {
      break;
    }
    case stop:
    {
      state.left = stop;
      break;
    }
    default:
    {
      break;
    }
  };

  //handle commands for right wheel
  switch(cmd_right_wheel)
  {
    case forward:
    {
      state.right = forward;
      break;
    }
    case back:
    {
      state.right = back;
      break;
    }
    case left:
    {
      break;
    }
    case right:
    {
      break;
    }
    case stop:
    {
      state.right = stop;
      break;
    }
    default:
    {
      break;
    }
  };
}

//timer
bool passed(unsigned long msec, unsigned long last_time_passed = last_time_timer)
{
  if(millis() - last_time_passed > msec)
  {
    last_time_timer = millis();
    return true;
  }
  return false;
}


void loop()
{
  if(radio_check(1))
  {
    //byte temp = 11;
    radio_read(1);
    handle_input();
    drive();
  }
  else
  {
    if(passed(500))
    {
      #if DEBUG_SERIAL
      //Serial.println("ROBOT NOTHING RECEIVED");
      #endif
    }
  }
  MLeft.run();
  MRight.run();
}