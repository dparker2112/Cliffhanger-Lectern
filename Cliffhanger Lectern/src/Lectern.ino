#include <Arduino.h>
#include "MillisTimer.h"

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
  7)Resets game to start position moves goat to starting position. No sound.
********************************Electromagnetic triggers***************************************
  An electromagnet creates a “start of game” sound when he crosses position one.
  An electromagnet at the top of the mountain would trigger a sound effect - falling yodel sound
  Perhaps another electromagnet could trigger red flashing lighting
    and a sound effect that warns that the goat is going to fall off when it passes.
*********************************Pinouts for Sound Board***************************************
  T01HOLDL.ogg                            Travel Music
  T02.ogg                                 Losing (Falling Yodel) Sound
  T03.ogg                                 Winning Sound
  T04.ogg                                 1 Ding
  T05.ogg                                 Danger Sound
  T06LATCH.ogg                            Idle Music
  T07.ogg                                 Buzz
  T08.ogg                                 Reset Game Music
*/
/*Serial1 pins 19(RX), 18(TX)*/
//********************************LECTERN BUTTONS*********************************************
const int reset = 4;       //Cue 7 : Moves goat to Start of game location : resetPin
const int randomMove = 5;  //Cue 1 : Moves goat random distance between resetPin and dangerPin
const int space24 = 6;     //Cue 2 : Moves goat to just before dangerPin (space 24)
const int manual = 7;      //Cue 3 : Moves goat until button is released
const int win = 8;         //Cue 4 : plays win sound
const int lose = 9;        //Cue 5 : plays lose sound
const int idle = 10;       //Cue 6 : plays win sound
//********************************SOUND TRIGGERS**********************************************
const int travelSoundPin = 30;  //pin 1 on sound board : Hold Looping Trigger
const int fallSoundPin = 31;    //pin 2 on sound board : Basic Trigger
const int winSoundPin = 32;     //pin 3 on sound board : Basic Trigger
const int dingSoundPin = 33;    //pin 4 on sound board : Basic Trigger
const int loseSoundPin = 34;    //pin 5 on sound board : Basic Trigger
const int idleSoundPin = 35;    //pin 6 on sound board : Latching Loop Trigger
const int dangerSoundPin = 36;  //pin 7 on sound board : Basic Trigger

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
  NONE = 4
} lectern_state_t;


typedef struct button_state_t {
  uint8_t reset;
  uint8_t random_move;
  uint8_t space24;
  uint8_t manual;
  uint8_t win;
  uint8_t lose;
  uint8_t idle;
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

void setup() {
  init_lectern_inputs();
  init_lectern_outputs();
  Serial.begin(115200);
  Serial1.begin(115200);
}

void loop() {
  read_inputs();
  handle_messages();
  display_error();
  play_sounds();
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
    buttonStates.space24 = digitalRead(space24);   //Cue 2 go to space 24
    buttonStates.manual = digitalRead(manual);    //Cue 3 manual move

    //SOUND CUES
    buttonStates.win = digitalRead(win);     //Cue 4 winning sound 1x
    if(buttonStates.win == LOW) {
      Serial.println("play win sound");
      playWinSound = true;
      //play win sound
    } 


    buttonStates.lose = digitalRead(lose);     //Cue 5 losing sound 1x
    if(buttonStates.lose == LOW) {
      Serial.println("play lose sound");
      playLoseSound = true;
      //play lose sound
    } 
    buttonStates.idle = digitalRead(idle);    //Cue 6 idle music loop
    if(buttonStates.idle == LOW) {
      Serial.println("play idle sound");
      playIdleSound = true;
      //play win sound
    } 

    //interpret button presses
    if(buttonStates.reset == LOW) {
      currentState = RESET;
      Serial.println("reset");
    } else if(buttonStates.random_move == LOW) {
      currentState = RANDOM_MOVE;
      Serial.println("random move");
    } else if(buttonStates.space24 == LOW) {
      currentState = SPACE24;
      Serial.println("space24");
    } else if(buttonStates.manual == LOW) {
      currentState = MANUAL;
      Serial.println("manual");
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
        delayMicroseconds(800);
        Serial1.write((byte *)&messageOut, sizeof(master_message_t));
        delayMicroseconds(800);
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
        //set error mode
        //go to transmit
        messageSendState = TRANSMIT;
        errorFlag = true;
        messageAck = false;
        Serial.println("no response");
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
              currentState = NONE;
            } else {
              Serial.println("message NACK");
              //if needed can add additional error state here
            }

            incomingWarningState = response.warningState;
            incomingEndState = response.endState;
            incomingTravellingState = response.travelState;
            Serial.print("warning: ");
            Serial.print(incomingWarningState);
            Serial.print(" end: ");
            Serial.print(incomingEndState);
            Serial.print(" travelling: ");
            Serial.println(incomingTravellingState);
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
  static bool travelSoundPlaying = false;
  static bool idleSoundPlaying = false;
  static bool idleButtonDebounced = true;
  static MillisTimer soundHoldResetTimer= { 500 };
  static bool soundHold = false;
  //handle the travel sound
  if(playTravelSound) {
    //play travel sound
    Serial.println("playing travel sound");
    digitalWrite(travelSoundPin, LOW);
    travelSoundPlaying = true;
  } else {
    if(travelSoundPlaying) {
      playTravelSound = false;
      playDingSound = true;
      travelSoundPlaying = false;
      Serial.println("turn off travel sound");
      digitalWrite(travelSoundPin, HIGH);
    }
  }

  //handle idle sound
  if(playIdleSound) {
    soundHoldResetTimer.reset();
    playIdleSound = false;
    if(idleButtonDebounced) {
      idleButtonDebounced = false;
      if(idleSoundPlaying) {
        Serial.println("turning off idle sound");
        digitalWrite(idleSoundPin, HIGH);
        idleSoundPlaying = false;
      } else {
        idleSoundPlaying = true;
        Serial.println("turning on idle sound");
        digitalWrite(idleSoundPin, LOW);
      }
    }
  }

  //handle the win sound
  if(playWinSound == true) {
      playWinSound = false;
      Serial.println("playing danger sound");
      digitalWrite(winSoundPin, LOW);
      soundHold = true;
      soundHoldResetTimer.reset();
  }


  //handle the lose sound
  if(playLoseSound == true) {
      playLoseSound = false;
      Serial.println("playing danger sound");
      digitalWrite(playLoseSound, LOW);
      soundHold = true;
      soundHoldResetTimer.reset();
  }

  //handle the danger sound
  if(playDangerSound == true) {
      playDangerSound = false;
      Serial.println("playing danger sound");
      digitalWrite(dangerSoundPin, LOW);
      soundHold = true;
      soundHoldResetTimer.reset();
  }

  //handle the fall sound
  if(playFallSound == true) {
      playFallSound = false;
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
      digitalWrite(loseSoundPin, HIGH);
      //reenable toggle for idle
      idleButtonDebounced = true;
    }
  }
}


//set pins as inputs
void init_lectern_inputs() {
  pinMode(reset, INPUT_PULLUP);       //Cue 7 return to home position
  pinMode(randomMove, INPUT_PULLUP);  //Cue 1 random move space 1 - 12
  pinMode(space24, INPUT_PULLUP);     //Cue 2 go to space 24
  pinMode(manual, INPUT_PULLUP);      //Cue 3 manual move
  pinMode(win, INPUT_PULLUP);         //Cue 4 winning sound 1x
  pinMode(lose, INPUT_PULLUP);        //Cue 5 losing sound 1x
  pinMode(idle, INPUT_PULLUP);        //Cue 6 idle music loop
}

//initialize outputs for triggering sound
void init_lectern_outputs() {
  pinMode(travelSoundPin, OUTPUT);
  pinMode(fallSoundPin, OUTPUT);
  pinMode(winSoundPin, OUTPUT);
  pinMode(dingSoundPin, OUTPUT);
  pinMode(loseSoundPin, OUTPUT);
  pinMode(idleSoundPin, OUTPUT);
  pinMode(dangerSoundPin, OUTPUT);

  pinMode(LED_BUILTIN, OUTPUT);  //built in led
  pinMode(max845_enable, OUTPUT);
  digitalWrite(max845_enable, LOW);
  //set audio signal pins to high
  digitalWrite(travelSoundPin, HIGH);
  digitalWrite(fallSoundPin, HIGH);
  digitalWrite(winSoundPin, HIGH);
  digitalWrite(dingSoundPin, HIGH);
  digitalWrite(loseSoundPin, HIGH);
  digitalWrite(idleSoundPin, HIGH);
  digitalWrite(dangerSoundPin, HIGH);
}