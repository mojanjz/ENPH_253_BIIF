#include <Arduino.h>
#include <stdlib.h>
#include <Wire.h>
#include <time.h>
#include <math.h>
#include <Adafruit_SSD1306.h>
#include <stack>

#include <search.h>
#include <approach.h>
#include <return.h>
#include <turn.h>
#include <turn180.h>
#include <sensors.h>
#include <followTape.h>
#include <strategy.h>

// TAPE FOLLOWER GLOBAL VARIABLE INITIALIZATION

unsigned long start_prev_error = 0; //Errors should all be 0 initially since robot won't begin following
unsigned long start_curr_error = 0; //tape unless both TFL and TFR are on the tape :)

volatile int previousError = 0; //The immediate previous sensed error we recieved
volatile int previousDiffError = 0; //The previous DIFFERENT error value that we were sensing

volatile int setMode = SEARCH; //Robot will initially be in search mode (following tape and looking for posts)

//TODO: Include Methanos / Thanos Switch!
int storageDirection = RIGHT; // CHANGE FOR TEAM
int initialTurn = RIGHT; //CHANGE FOR TEAM

bool forkPathCrossed = false; // have we crossed the tape completely

bool pingSlave = false;

float speedFactor = 0.38;

void debugSensorReadings(int setMode);
void exitModeAlerts(int setMode);

int numForksTaken = 0;
int forksInPath = 0;
int TEAM = METHANOS;

//course navigation
std :: stack <int> forkHistory;
int currentPostMap[6] = {0};

const int TRIG_R = PB14;
const int ECHO_R = PB15;
const int TRIG_L = PB12;
const int ECHO_L = PB13;
int postLineUpTimer = 0;

//LED timer
const int LEFT_FORK_LED = PB11;
const int RIGHT_FORK_LED = PB10;
int ledTimer = 0;

// ramp sensor
const int rampSensor = PB1;
int rampSensorHistory = 2000;
int testArray [2] = {0};
int arrayCopy = 0; //delete later

int speedTimer = 0; // to decrease the speed after a certain amount of time

int stateBeforeTurn = 0;
int stateBefore180Turn = 0;

int forkTimer = 0; //to reduce chances of double counting the forks
int turnTimer = 0; //keeps turning before checking for the line
int followTapeTimer = 0;
int forkDetectionCondition = 1;

void setup() {

  Serial.begin(9600);
  //DO NOT inlucde Wire.begin() --> Interferes with the TX&RX functions of the bluepill!

  //Delay added for debugging (allows time to catch beginning of Serial Monitor)
  //Delete for competition
  delay(5000);

  // TEAM
  pinMode(WHAT_TEAM,INPUT);

 // TAPE FOLLOWER
  pinMode(DETECT_THRESHOLD, INPUT);
  pinMode(LIGHT_SENSOR,INPUT);
  pinMode(TAPE_FOLLOWER_L,INPUT);
  pinMode(TAPE_FOLLOWER_R,INPUT);

  pinMode(LEFT_FORK_LED,OUTPUT);
  pinMode(RIGHT_FORK_LED,OUTPUT);


  //POST DETECTORS
  pinMode(FORK_SENSOR_L, INPUT);
  pinMode(FORK_SENSOR_R, INPUT);

  //WHEEL MOTOR PWMs
  pinMode(LEFT_WHEEL_FWD, OUTPUT);
  pinMode(LEFT_WHEEL_BKWD, OUTPUT);
  pinMode(RIGHT_WHEEL_FWD, OUTPUT);
  pinMode(RIGHT_WHEEL_BKWD, OUTPUT);

 //RAMP SENSOR
  pinMode(rampSensor,INPUT_PULLUP);

  //COLLIOSN SENSOR
  pinMode(COLLISION_SWITCH,INPUT_PULLUP);

  //SET THRESHOLD
  float threshold = analogRead(DETECT_THRESHOLD);

  //Do not uncomment before figuring out what the fuck is going on (the pins are weird )
   pinMode(ECHO_L, INPUT);
   pinMode(TRIG_L, OUTPUT);
   pinMode(ECHO_R, INPUT);
   pinMode(TRIG_R, OUTPUT);

  //TODO: SWTICH IS OPEN CHANGE TEAM TO THANOS
  if(false) {
    TEAM = THANOS;
  }

  forksInPath = initializeStrategy(TEAM);


  //Check to see if robot is initially on tape
  int initialCondition = 0;
  int counter = 0;
  // while(analogRead(TAPE_FOLLOWER_L) < threshold || analogRead(TAPE_FOLLOWER_R) < threshold) {
  //   if(initialCondition == 0) {
  //     initialCondition++;
  //     Serial.println("Robot Initially off tape, please fix :)");
  //   }
  //   counter++;
  //   if(counter % 10000 == 0) {
  //     Serial.print("Threshold: ");
  //     Serial.print(analogRead(DETECT_THRESHOLD));
  //     Serial.print("Left Sensor Value: ");
  //     Serial.print(analogRead(TAPE_FOLLOWER_L));
  //     Serial.print(" | Right Sensor Value: ");
  //     Serial.println(analogRead(TAPE_FOLLOWER_R));
  //   }
  // }

  //Start driving the robot forward once the initial conditions have been meet
  pwm_start(LEFT_WHEEL_FWD, 100000, SPEED, 0, 1);
  pwm_start(RIGHT_WHEEL_FWD, 100000, SPEED, 0, 1);
  pwm_start(LEFT_WHEEL_BKWD, 100000, SPEED, 0, 1);
  pwm_start(RIGHT_WHEEL_BKWD, 100000, SPEED, 0, 1);
  startDriving();
  Serial.println("Setup Completed");
  speedTimer = millis();
}



//MAIN STATE SWITCH BOX
void loop() {
  #ifdef TEAM_TESTING
    Serial.println(analogRead(WHAT_TEAM));

    if(analogRead(WHAT_TEAM)  > 1000){
      Serial.println("THANOS");
    } else {
      Serial.println("METHANOS");
    }
  #endif

  #ifdef COLLISION_TESTING
   if(digitalRead(COLLISION_SWITCH) == 1){
     Serial.println("COLLISION");
   } else {
     Serial.println("NO COLLISION");
   }
   #endif



  if(millis() - ledTimer > 1000){
    //Serial.println("flashing the led");
    digitalWrite(LEFT_FORK_LED,LOW);
    digitalWrite(RIGHT_FORK_LED,LOW);
    ledTimer = 0;
  }

  // if(millis() - speedTimer > 9600 ) {
  //   speedFactor = 0.24;
  //   forkDetectionCondition = 5; //can detect forks better on the surface
  // } //CHANGE

  int rampValue = digitalRead(rampSensor);
  if(rampValue == 1) {
    #ifdef RAMP_TESTING
      Serial.println("slowing down the robot");
      Serial.print(rampValue);
      Serial.print(" | history:");
      Serial.println(rampSensorHistory);
    #endif
    speedFactor = 0.18;
    forkDetectionCondition = 5; // can detect forks better on the surface
  }
  rampSensorHistory = analogRead(rampSensor);

  //To see the values for all of the sensors, uncomment the next line
  #ifdef RAMP_TESTING
    Serial.println(analogRead(rampSensor));
  #endif
  #ifdef TESTING
  debugSensorReadings(setMode);
  #endif
  switch (setMode) {
    case SEARCH:
      setMode = searchMode();
      break;
    case APPROACH:
      //setMode = approachMode();
      break;
    case RETRIEVE_L:
      while (millis() - postLineUpTimer < 270) {
        followTape();
      }
      stopRobot();
      stateBefore180Turn = RETRIEVE_L;
      if(millis() - postLineUpTimer > 5000){
        setMode = TURN_R_180;
      }
      // Serial.print("Current Path Map: ");
      // for(int i = 0; i < forksInPath; i++) {
      //   if(currentPostMap[i] == LEFT) {
      //     Serial.print("LEFT ");
      //   } else if(currentPostMap[i] == RIGHT) {
      //     Serial.print("RIGHT ");
      //   } else {
      //     Serial.print("OVERCOUNTED ");
      //   }
      // }
      // Serial.print(" |  Number of Forks detected: ");
      // Serial.print(numForksTaken);
      // Serial.print(" | Fork History: ");
      // while(forkHistory.size() != 0) {
      //   testArray[arrayCopy] = forkHistory.top(); //NOTE THIS IS BACKWARDS!!!
      //   forkHistory.pop();
      // }
      // for(int i = 0; i < sizeof testArray / sizeof testArray[0]; i++) {
      //   if(testArray[i] == LEFT) {
      //     Serial.print("LEFT ");
      //   } else if(testArray[i] == RIGHT) {
      //     Serial.print("RIGHT ");
      //   } else {
      //     Serial.print("OVERCOUNTED ");
      //   }
      // }
      // Serial.println("");
      //setMode = retrieveMode();
      break;
    case RETRIEVE_R:
      while (millis() - postLineUpTimer < 270) {
        followTape();
      }
      stopRobot();
      stateBefore180Turn = RETRIEVE_R;
      if(millis() - postLineUpTimer > 2000){
        setMode = TURN_L_180;
      }
      break;
    case PATHFINDER:
      //setMode = pathfinderMode();
      break;
    case RETURN:
      //setMode = returnMode();
      stopRobot(); //change
      break;
    case DEPOSIT:
      //setMode = depositMode();
      stopRobot();
      break;
    case DEFENSE:
      //setMode = defenseMode();
      break;
    case TURN_L:
     //setMode = turnMode(TURN_L);
     //Serial.println("Found fork on the left, turning to left");
     //delay(1000);
     setMode = turnMode(TURN_L);
     //setMode = SEARCH;
     break;
    case TURN_R:
      //Serial.println("Found fork on the right, turning to right");
      //delay(1000);
      setMode = turnMode(TURN_R);
      break;
    case TURN_L_180:
      setMode = turn180Robot(TURN_L_180);
      break;
    case TURN_R_180:
      setMode = turn180Robot(TURN_R_180);
      break;

  }
  //To debug and see the mode that the robot exited with, uncomment the next line
  //exitModeAlerts(setMode);
}



/* debugSensorReadings()
 *
 * Prints out the sensor readings for all of the QRD sensors on the front of the robot
 * For debugging purposes only :)
 * @param : setMode - the current mode/state of the robot
 */
void debugSensorReadings(int setMode) {
  if(millis() % 500 == 0) {
    Serial.print(" | FSL Value: ");
    Serial.print(analogRead(FORK_SENSOR_L));
    Serial.print(" | TFL Value: ");
    Serial.print(analogRead(TAPE_FOLLOWER_L));
    Serial.print(" | TFR Value: ");
    Serial.print(analogRead(TAPE_FOLLOWER_R));
    Serial.print(" | FSR Value: ");
    Serial.print(analogRead(FORK_SENSOR_R));
    Serial.print(" | THRESH Value: ");
    Serial.print(analogRead(DETECT_THRESHOLD));
    Serial.print(" | FORK THRESHOLD: ");
    Serial.print(analogRead(DETECT_THRESHOLD)+ FORK_THRESHOLD_OFFSET);
    Serial.print(" | MODE: ");
    Serial.println(setMode);
  }
}



/* exitModeAlerts()
 *
 * Prints the mode that the robot exited the loop with
 * For debugging purposes only :)
 * @param : setMode - the current mode/state of the robot
 */
void exitModeAlerts(int setMode) {
  Serial.print("Exited with mode: ");
  Serial.println(setMode);
}