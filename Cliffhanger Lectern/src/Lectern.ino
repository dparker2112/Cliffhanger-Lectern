#include <Arduino.h>
#include "MillisTimer.h"
#include "ledPatterns.h" 
// Control for Cliffhanger game.  Made by David Parker nlightn0@gmail.com 04/01/2022

/*
*******************************Overview of button behaviors*************************************
  1)Start button: Random goat move 1-12 spaces along the track. Each move different.
  Beginning yodel to start, then Traveling music track plays during movement. When the figure stops, a
  bell “DING”
  2)Go directly to space #24. Beginning yodel to start, then plays music while it travels and
  when the figure stops a bell “Ding”
  3)Manual run. Beginning yodel to start, character moves until host takes finger off button then the stop DING
  4)Sound track win..play sound ding ding ding ding and winning music
  5)Sound track lose…loosing sound, and music
  6)Idle music.
  7)Resets game to start position moves goat to starting position. Plays Reset Game Music.
********************************Electromagnetic triggers***************************************
  An electromagnet creates a “start of game” sound when he crosses position one.
  An electromagnet at the top of the mountain would trigger a sound effect - falling yodel sound
  Perhaps another electromagnet could trigger red flashing lighting
    and a sound effect that warns that the goat is going to fall off when it passes.
*********************************Pinouts for Sound Board***************************************
  T01HOLDL.ogg                            Travel Music - loops until pin HIGH 
  T02.ogg                                 Losing (Falling Yodel) Sound
  T03.ogg                                 Winning Sound
  T04.ogg                                 1 Ding
  T05.ogg                                 Danger Sound
  T06HOLDL.ogg                            Idle Music - loops until pin HIGH
  T07.ogg                                 Buzz
  T08.ogg                                 Reset Game Music
*/
/*Serial1 pins 19(RX), 18(TX)*/
//********************************LECTERN BUTTONS*********************************************
const int reset = 4;       //Cue 7 : Moves goat to Start of game location : resetPin
const int randomMove = 5;  //Cue 1 : Moves goat random distance between resetPin and dangerPin
const int dingSoundTriggerPin = 6;     //Cue 2 : Moves goat to just before dangerPin (space 24)
const int manual = 7;      //Cue 3 : Moves goat until button is released
const int win = 8;         //Cue 4 : plays win sound
const int buzz = 9;        //Cue 5 : plays buzz sound
const int idle = 10;       //Cue 6 : plays idle music
const int moveOne = 11;    //Cue 8 : Moves goat one space forward
const int secondFunctionButton = 12;    //Cue 9
//********************************SOUND TRIGGERS**********************************************
const int travelSoundPin = 30;   //pin 1 on sound board : Latching Looping Trigger
const int fallSoundPin = 31;     //pin 2 on sound board : Basic Trigger
const int winSoundPin = 32;      //pin 3 on sound board : Basic Trigger
const int dingSoundPin = 33;     //pin 4 on sound board : Basic Trigger
const int dangerSoundPin = 34;   //pin 5 on sound board : Basic Trigger
const int idleSoundPin = 35;     //pin 6 on sound board : Latching Loop Trigger
const int buzzSoundPin = 36;     //pin 7 on sound board : Basic Trigger
const int resetSoundPin = 37;    //pin 8 on sound board : Basic Trigger

const int fallSoundLength = 500; //length of fall sound in ms

const int max845_enable = 2;

typedef enum message_status_t {
  ACK = 0xAA,
  NACK = 0xCC
} message_status_t;

typedef enum message_send_state_t {
  TRANSMIT,
  RECEIVE
} message_send_state_t;

typedef enum lectern_state_t {
  RESET = 0,
  RANDOM_MOVE = 1,
  SPACE24 = 2,
  MANUAL = 3,
  NONE = 4,
  NO_MESSAGE = 5,
  MOVE_1 = 6
} lectern_state_t;


typedef struct button_state_t {
  uint8_t reset;
  uint8_t random_move;
  uint8_t dingSound;
  uint8_t manual;
  uint8_t win;
  uint8_t lose;
  uint8_t idle;
  uint8_t buzz;
  uint8_t moveOne;
  uint8_t secondFunction;
} button_state_t;

button_state_t buttonStates = {1};

typedef struct master_message_t {
  uint8_t startByte;
  lectern_state_t currentState;
  uint8_t endByte;
} master_message_t;

typedef struct response_message_t {
  uint8_t startByte;
  uint8_t warningState;
  uint8_t endState;
  uint8_t travelState;
  message_status_t messageStatus;
  uint8_t endByte;
} response_message_t;


uint8_t startByte = 0x81;
uint8_t endByte = 0x7E;
lectern_state_t currentState = NONE;
lectern_state_t previousState = NONE;

message_send_state_t messageSendState = TRANSMIT;

MillisTimer messageResponseTimeoutTimer = { 120 };
MillisTimer messageSendTimer = { 50 };
MillisTimer messageErrorIndicatorTimer = { 1000 };


bool ledState = false;
bool errorFlag = false;
bool messageAck = false;

uint8_t incomingWarningState = 0;
uint8_t incomingEndState = 0;
uint8_t incomingTravellingState = 0;

bool playIdleSound = false;
bool playWinSound = false;
bool playLoseSound = false;
bool playDingSound = false;
bool playWarningSound = false;
bool playTravelSound = false;
bool playDangerSound = false;
bool playFallSound = false;
bool playResetSound = false;
bool playBuzzSound = false;

bool gameOver = false;
bool travelSoundPlaying = false;
bool isReset = false;

void setup() {
  init_lectern_inputs();
  init_lectern_outputs();
  initLEDs();
  Serial.begin(115200);
  Serial1.begin(115200);
  //while(!Serial);

  
  //uncomment one of these lines to test the pattern
  //startLosePattern();
  //startWinPattern();
  //startWarningPattern();
  
  //uncomment one of these lines to test the pattern with the sound
  //playDangerSound = true;
  //playFallSound = true;
  //playWinSound = true;
}

void loop() {
  read_inputs();
  handle_messages();
  display_error();
  play_sounds();
  ledLoop();
}

//blinks led when communication is broken
void display_error() {
  static bool ledState = LOW;
  if (errorFlag) {
    if (messageErrorIndicatorTimer.timeUp()) {
      messageErrorIndicatorTimer.reset();
      //toggle led
      ledState = !ledState;
      digitalWrite(LED_BUILTIN, ledState);
    }
  } else if (ledState == HIGH) {
    ledState = LOW;
    digitalWrite(LED_BUILTIN, LOW);
    //if led on, turn off
  }
}

//polls the buttons to get current inputs
void read_inputs() {
  static MillisTimer inputTimer = {30};
  if(inputTimer.timeUp()) {
    inputTimer.reset();
    buttonStates.reset = digitalRead(reset);     //Cue 7 return to home position
    buttonStates.random_move = digitalRead(randomMove);//Cue 1 random move space 1 - 12
    buttonStates.secondFunction = digitalRead(secondFunctionButton);   //Cue 2 go to space 24
    buttonStates.manual = digitalRead(manual);    //Cue 3 manual move
    buttonStates.moveOne = digitalRead(moveOne);
    buttonStates.dingSound = digitalRead(dingSoundTriggerPin);

    //SOUND CUES
    buttonStates.win = digitalRead(win);     //Cue 4 winning sound 1x
    if(buttonStates.win == LOW) {
      Serial.println("play win sound");
      playWinSound = true;
      //play win sound
    } 

  /*
    buttonStates.lose = digitalRead(lose);     //Cue 5 losing sound 1x
    if(buttonStates.lose == LOW) {
      Serial.println("play lose sound");
      playLoseSound = true;
      //play lose sound
    }
    */ 
    buttonStates.idle = digitalRead(idle);    //Cue 6 idle music loop
    if(buttonStates.idle == LOW) {
      Serial.println("play idle sound");
      playIdleSound = true;
      //play win sound
    } 

    buttonStates.buzz = digitalRead(buzz);    //Cue 5 buzz sound
    if(buttonStates.buzz == LOW) {
      Serial.println("play buzz sound");
      playBuzzSound = true;
      //play buzz sound
    } 


    //interpret button presses
    if(buttonStates.reset == LOW) {
      gameOver = false;
      currentState = RESET;
      isReset = true;
      //Serial.println("reset");
      Serial.println("play reset sound");
      playResetSound = true;
    } else if(buttonStates.random_move == LOW && buttonStates.secondFunction == HIGH && !gameOver) {
      currentState = RANDOM_MOVE;
      Serial.println("random move");
      isReset = false;
    } else if(buttonStates.random_move == LOW && buttonStates.secondFunction == LOW && !gameOver) {
      currentState = SPACE24;
      Serial.println("space24");
      isReset = false;
    } else if(buttonStates.manual == LOW && !gameOver) {
      currentState = MANUAL;
      Serial.println("manual");
      isReset = false;
    } else if(buttonStates.moveOne == LOW && !gameOver) {
      currentState = MOVE_1;
      Serial.println("move 1 space");
      isReset = false;
    } else if(buttonStates.dingSound == LOW) {
      currentState = NONE;
      Serial.println("play ding sound");
      playDingSound = true;

    }else {
      currentState = NONE;
    }
    
  }
}

/*
  sends message, and listens for response
  in case of nack, resends message, in case of ack
  clears message, in case of timeout enters error mode
  exits error mode when connection is reestablished
*/
void handle_messages() {
  master_message_t messageOut = { startByte, currentState, endByte };
  response_message_t response;
  switch (messageSendState) {
    case TRANSMIT:
      if (messageSendTimer.timeUp()) {
        messageSendTimer.reset();
        if (errorFlag) {
          //resend message
          //can use this to send message again if necessary
        } else {
          //send new message
        }
        digitalWrite(max845_enable, HIGH);
        delayMicroseconds(1000);
        if(currentState == RESET) {
          Serial.print("reset");
        }
        Serial1.write((byte *)&messageOut, sizeof(master_message_t));
        delayMicroseconds(1000);
        digitalWrite(max845_enable, LOW);
        /*
        Serial.print("message sent ");
        Serial.print(startByte);
        Serial.print(" ");
        Serial.print(currentState);
        Serial.print(" ");
        Serial.println(endByte);
        */
        
        messageSendState = RECEIVE;
        //transmit message

        messageResponseTimeoutTimer.reset();
      }


      break;
    case RECEIVE:
      if (messageResponseTimeoutTimer.timeUp()) {
        messageResponseTimeoutTimer.reset();
        //set error mode
        //go to transmit
        messageSendState = TRANSMIT;
        errorFlag = true;
        messageAck = false;
        Serial.print("no response");
        while(Serial1.available()) {
          delayMicroseconds(10);
          Serial.print(Serial1.read(), DEC);
          Serial.print(" ");
        }
        Serial.println();
        
      } else {
        if ((unsigned)Serial1.available() >= sizeof(response_message_t)) {
          // Read in the appropriate number of bytes to fit the response
          Serial1.readBytes((byte *)&response, sizeof(response_message_t));
          //verify response start and end bytes
          if (response.startByte == startByte && response.endByte == endByte) {
            messageSendState = TRANSMIT;
            //if message valid turn error flag off
            errorFlag = false;
            //if message ack set to none state
            if (response.messageStatus == ACK) {
              //errorFlag = true;
              messageAck = true;
              //Serial.println("message ACK");
              //currentState = NONE;
            } else {
              Serial.println("message NACK");
              if(currentState == RESET) {
                //currentState = NONE;
              }
              //if needed can add additional error state here
            }

            incomingWarningState = response.warningState;
            incomingEndState = response.endState;
            incomingTravellingState = response.travelState;
            
            //Serial.print("trav sound: ");
            //Serial.print(travelSoundPlaying);
            Serial.print("warning: ");
            Serial.print(incomingWarningState);
            Serial.print(" end: ");
            Serial.print(incomingEndState);
            Serial.print(" travelling: ");
            Serial.print(incomingTravellingState);
            Serial.print(" gameover: ");
            Serial.print(gameOver);
            Serial.println();
            
            playFallSound = incomingEndState;
            playDangerSound = incomingWarningState;
            playTravelSound = incomingTravellingState;
            
          }
        }
      }
      break;
  }
}

/*
* this functions handles sound effects, when pins pulled low, sound plays
* travel sound is hold-looping trigger, while idle sound is latching trigger
*/
void play_sounds() {
  //static bool travelSoundPlaying = false;
  static bool idleSoundPlaying = false;
  static bool idleButtonDebounced = true;
  static MillisTimer soundHoldResetTimer= { 500 };
  static bool soundHold = false;
  static bool resetPlaying = false;
  //handle the travel sound
  if(playTravelSound && !playFallSound && !gameOver && !isReset) {
    //play travel sound
    if(!travelSoundPlaying) {
      Serial.println("playing travel sound");
      digitalWrite(travelSoundPin, LOW);
      travelSoundPlaying = true;
      //soundHold = true;
      //soundHoldResetTimer.reset();
    }

  } else {
    if(travelSoundPlaying) {
      playTravelSound = false;
      playDingSound = true;
      travelSoundPlaying = false;
      Serial.println("turn off travel sound");
      digitalWrite(travelSoundPin, HIGH);
      delay(50);
      //soundHold = true;
      //soundHoldResetTimer.reset();
    }
  }

  //handle idle sound
  if(playIdleSound) {
    playIdleSound = false;
    if(idleButtonDebounced) {
      
      if(idleSoundPlaying) {
        idleSoundPlaying = false;
        digitalWrite(idleSoundPin, HIGH);
        Serial.println("idle sound stopped");
      } else {
        idleSoundPlaying = true;
        digitalWrite(idleSoundPin, LOW);
        Serial.println("idle sound started");
      }
      idleButtonDebounced = false;
      soundHold = true;
      soundHoldResetTimer.reset();
      
    }
  }

  //handle the win sound
  if(playWinSound == true) {
      startWinPattern();
      playWinSound = false;
      Serial.println("playing win sound");
      digitalWrite(winSoundPin, LOW);
      soundHold = true;
      soundHoldResetTimer.reset();
  }

    //handle the Buzz sound
  if(playBuzzSound == true) {
      playBuzzSound = false;
      Serial.println("playing buzz sound");
      digitalWrite(buzzSoundPin, LOW);
      soundHold = true;
      soundHoldResetTimer.reset();
  }

  //handle the reset sound
  if(playResetSound == true && !resetPlaying) {
      playResetSound = false;
      resetPlaying = true;
      Serial.println("playing reset sound"); 
      digitalWrite(resetSoundPin, LOW);
      soundHold = true;
      soundHoldResetTimer.reset();
  }


  //handle the danger sound
  if(playDangerSound == true) {
      startWarningPattern();
      playDangerSound = false;
      if(travelSoundPlaying) {
        playTravelSound = false;
      }

      delay(4);
      Serial.println("playing danger sound"); 
      digitalWrite(dangerSoundPin, LOW);
      soundHold = true;
      soundHoldResetTimer.reset();
  }

  //handle the fall sound
  if(playFallSound == true && gameOver == false) {
      //disable travel sound
      startLosePattern();
      if(travelSoundPlaying) {
        playTravelSound = false;
      }

      delay(4);
      playFallSound = false;
      gameOver = true;
      Serial.println("playing fall sound");
      digitalWrite(fallSoundPin, LOW);

      soundHold = true;
      soundHoldResetTimer.reset();
  }

  //handle the ding sound
  if(playDingSound == true) {
      playDingSound = false;
      Serial.println("playing ding sound");
      digitalWrite(dingSoundPin, LOW);
      soundHold = true;
      soundHoldResetTimer.reset();
  }

  if(soundHold) {
    if(soundHoldResetTimer.timeUp()) {
      soundHold = false;
      //return hold pins to high
      digitalWrite(dingSoundPin, HIGH);
      digitalWrite(fallSoundPin, HIGH);
      digitalWrite(dangerSoundPin, HIGH);
      digitalWrite(winSoundPin, HIGH);
      //digitalWrite(loseSoundPin, HIGH);
      //digitalWrite(idleSoundPin, HIGH);
      digitalWrite(resetSoundPin, HIGH);
      resetPlaying = false;
      digitalWrite(buzzSoundPin, HIGH);
      digitalWrite(travelSoundPin, HIGH);
      //reenable toggle for idle
      idleButtonDebounced = true;
    }
  }
}


//set pins as inputs
void init_lectern_inputs() {
  pinMode(reset, INPUT_PULLUP);       //Cue 7 return to home position
  pinMode(randomMove, INPUT_PULLUP);  //Cue 1 random move space 1 - 12
  pinMode(secondFunctionButton, INPUT_PULLUP);     //Cue 2 go to space 24
  pinMode(manual, INPUT_PULLUP);      //Cue 3 manual move
  pinMode(win, INPUT_PULLUP);         //Cue 4 winning sound 1x
  pinMode(dingSoundTriggerPin, INPUT_PULLUP);
  //pinMode(lose, INPUT_PULLUP);        //Cue 5 losing sound 1x
  pinMode(buzz, INPUT_PULLUP);        //Cue 5 losing sound 1x
  pinMode(idle, INPUT_PULLUP);        //Cue 6 idle music loop
  pinMode(moveOne, INPUT_PULLUP);        //Cue 8 move one step forward
}

//initialize outputs for triggering sound
void init_lectern_outputs() {
  pinMode(travelSoundPin, OUTPUT);
  pinMode(fallSoundPin, OUTPUT);
  pinMode(winSoundPin, OUTPUT);
  pinMode(dingSoundPin, OUTPUT);
  pinMode(resetSoundPin, OUTPUT);
  pinMode(idleSoundPin, OUTPUT);
  pinMode(dangerSoundPin, OUTPUT);
  pinMode(buzzSoundPin, OUTPUT);

  pinMode(LED_BUILTIN, OUTPUT);  //built in led
  pinMode(max845_enable, OUTPUT);
  digitalWrite(max845_enable, LOW);
  //set audio signal pins to high
  digitalWrite(travelSoundPin, HIGH);
  digitalWrite(fallSoundPin, HIGH);
  digitalWrite(winSoundPin, HIGH);
  digitalWrite(dingSoundPin, HIGH);
  digitalWrite(resetSoundPin, HIGH);
  digitalWrite(idleSoundPin, HIGH);
  digitalWrite(dangerSoundPin, HIGH);
  digitalWrite(buzzSoundPin, HIGH);
}