#include <Bluepad32.h>

// ***** This uses arcade style driving where right trigger is for throttle, left trigger is for break, 
// left joystick is for turning only, O button is for emergency break toggle, X button is for weapon ****
int rightMotorPin = 12;
int leftMotorPin = 14;
int weaponMotorPin = 18;

int resolutionBits = 14; 
int frequency = 50; 
int maxDuty = (1 << resolutionBits) - 1; 

// Old Constants - Had jamming with these 
int backwardsMin = 820;  // ~1.0ms (Max Reverse / Weapon OFF)
int backwardsMax = 1150; // ~1.4ms (Slight Reverse)
int neutral      = 1228; // ~1.5ms (Neutral/Stop)
int forwardMin   = 1310; // ~1.6ms (Slight Forward)
int forwardMax   = 1638; // ~2.0ms (Max Forward)

const int rightMotorChannel = 4; 
const int leftMotorChannel = 5;
const int weaponMotorChannel = 6;

int currentLeftDuty = neutral; 
int currentRightDuty = neutral;

// --- DRIVE RAMPING VARIABLES ---
// Tune these to change how the bot feels!
float accelStep = 3.0f;  // Speed of acceleration (lower = smoother/slower)
float decelStep = 15.0f; // Speed of slowing down (higher = snappier braking)

float currentLeftPercent = 0.0f; 
float currentRightPercent = 0.0f;

// --- WEAPON RAMPING & LIMITS ---
// Lower this number if the weapon is still too terrifying at max speed.
// forwardMax is 1638. Try 1400 or 1500 for a safer top speed.
int weaponMaxSpeed = 1450; 

// Tune this to change how fast the weapon spools up.
// A lower number means a slower, safer spin-up.
float weaponAccelStep = 4.0f; 

float currentWeaponDuty = backwardsMin; // Start at 820 (OFF)

// --- NEW E-STOP VARIABLES ---
bool eStopActive = false;
bool lastEStopButtonState = false;

// --- NEW TIMING VARIABLES ---
unsigned long lastMotorUpdate = 0;
const int updateInterval = 20; // 50Hz update loop (20ms)

ControllerPtr myGamePad = nullptr;  

// ****Bluetooth Logic****
void initalizeBluepad32() {
   BP32.setup(&onConnectedController, &onDisconnectedController);
   BP32.enableVirtualDevice(false);
}

void onConnectedController(ControllerPtr controller) {
  Serial.println("Controller Connected"); 
  myGamePad = controller;
}

void onDisconnectedController(ControllerPtr controller) {
  Serial.println("Controller Disconnected"); 
  if (myGamePad == controller) {
    myGamePad = nullptr;
    emergencyBreak(); // Safely stop motors if disconnected
  }
}

// ****Motor controlling logic****
void intializeMotors() {
  // Drive Motors
  ledcSetup(rightMotorChannel, frequency, resolutionBits); 
  ledcAttachPin(rightMotorPin, rightMotorChannel); 

  ledcSetup(leftMotorChannel, frequency, resolutionBits);   
  ledcAttachPin(leftMotorPin, leftMotorChannel); 

  // Weapon Motor
  ledcSetup(weaponMotorChannel, frequency, resolutionBits);
  ledcAttachPin(weaponMotorPin, weaponMotorChannel);

  // ARM THE ESCs
  ledcWrite(rightMotorChannel, neutral);
  ledcWrite(leftMotorChannel, neutral);
  
  // FIXED: ESCs need a ~1.0ms pulse to arm, NOT a 0 duty cycle
  ledcWrite(weaponMotorChannel, backwardsMin); 
  delay(2000); // 2 seconds to arm
}

int getMotorSpeed(float speedPercent, int direction) {
  if (direction == 0) {
    return int(speedPercent * 0.01f * (backwardsMin - backwardsMax) + backwardsMax);
  }
  else if (direction == 1) {
    return int(speedPercent * 0.01f * (forwardMax - forwardMin) + forwardMin);
  }
  else {
    return neutral;
  }
}

int percentToDuty(float percent) {
  if (percent > 0) {
    return getMotorSpeed(percent, 1);
  } 
  else if (percent < 0) {
    return getMotorSpeed(-percent, 0); 
  }
  return neutral;
}

void writeMotor(int motorChannel, float percent) {
  if (motorChannel == rightMotorChannel) {
    percent = -percent;
  }
  int duty = percentToDuty(percent);
  ledcWrite(motorChannel, duty);
}

void motorNeutral(int motorChannel) {
  ledcWrite(motorChannel, getMotorSpeed(0, 3)); 
}

void emergencyBreak() {
    motorNeutral(leftMotorChannel); 
    motorNeutral(rightMotorChannel);
    
    // Output the 1.0ms OFF signal to safely stop the weapon ESC
    ledcWrite(weaponMotorChannel, backwardsMin); 
    
    currentLeftPercent = 0.0f;
    currentRightPercent = 0.0f;
    currentWeaponDuty = backwardsMin; // Reset weapon ramp state
}

float getThrottlePercent() {
  if (myGamePad) {
    return float(myGamePad->throttle()) / 1023.0f * 100.0f;
  }
  return 0.0f;
}

float getBrakePercent() {
  if (!myGamePad) return 0.0f;
  return float(myGamePad->brake()) / 1023.0f * 100.0f;
}

float getTurnPercent() {
  if (!myGamePad) return 0.0f; 
  return float(myGamePad->axisX()) / 512.0f * 100.0f; 
}

// Updated Ramping logic with snappy deceleration
float rampToward(float current, float target) {
  float step = accelStep; 
  
  // If target is closer to 0 than current, or we are crossing 0 (reversing), use the snappy deceleration step
  if (abs(target) < abs(current) || (target * current < 0)) {
    step = decelStep;
  }

  if (current < target) {
    current = current + step; 
    if (current > target) {
      current = target;
    }
  }
  else if (current > target) {
    current = current - step;
    if (current < target) {
      current = target;
    }
  }
  return current;
}

void applyTurning(float basePercent, float turnPercent) {
  float scaledTurn = turnPercent * 0.5f; 
  
  float leftTarget = basePercent + scaledTurn; 
  float rightTarget = basePercent - scaledTurn; 

  leftTarget = constrain(leftTarget, -100.0f, 100.0f);
  rightTarget = constrain(rightTarget, -100.0f, 100.0f);

  currentLeftPercent = rampToward(currentLeftPercent, leftTarget); 
  currentRightPercent = rampToward(currentRightPercent, rightTarget);

  writeMotor(leftMotorChannel, currentLeftPercent);
  writeMotor(rightMotorChannel, currentRightPercent);
}

void updateDrive() {
  if (!myGamePad) {
    emergencyBreak();
    return;
  }

  // --- E-STOP TOGGLE LOGIC ---
  bool currentEStopBtn = (myGamePad->buttons() & (1<<1)); // O button

  // If button was just pushed down this exact frame
  if (currentEStopBtn && !lastEStopButtonState) {
    eStopActive = !eStopActive; // Toggle the state
    if (eStopActive) {
      Serial.println("E-STOP ACTIVATED!");
    } else {
      Serial.println("E-STOP DEACTIVATED.");
    }
  }
  lastEStopButtonState = currentEStopBtn;

  // If E-Stop is active, freeze the bot and skip calculating drive
  if (eStopActive) {
    emergencyBreak();
    return;
  }

  // Get inputs
  float throttle = getThrottlePercent();
  float brake = getBrakePercent(); 
  float turn = getTurnPercent();

  float deadzone = 15.0f; 
  if (throttle < deadzone) throttle = 0.0f;
  if (brake < deadzone) brake = 0.0f;
  if (abs(turn) < deadzone) turn = 0.0f;

  float base = throttle - brake; 
  base = constrain(base, -100.0f, 100.0f);

  applyTurning(base, turn); 
}

void handleWeapon() {
  float targetWeaponDuty = backwardsMin; // Default to OFF

  // If connected, not E-Stopped, and X is pressed, set target to our capped max speed
  if (myGamePad && !eStopActive && (myGamePad->buttons() & 0x04)) {
    targetWeaponDuty = weaponMaxSpeed;
  }

  // --- RAMPING LOGIC ---
  if (currentWeaponDuty < targetWeaponDuty) {
    // Spooling up
    currentWeaponDuty += weaponAccelStep;
    if (currentWeaponDuty > targetWeaponDuty) {
      currentWeaponDuty = targetWeaponDuty;
    }
  } 
  else if (currentWeaponDuty > targetWeaponDuty) {
    // Spinning down. We usually want weapons to spin down faster than they spin up
    // so we multiply the step to drop the throttle quicker.
    currentWeaponDuty -= (weaponAccelStep * 5.0f); 
    if (currentWeaponDuty < targetWeaponDuty) {
      currentWeaponDuty = targetWeaponDuty;
    }
  }

  // Send the calculated ramped duty cycle to the ESC
  ledcWrite(weaponMotorChannel, (int)currentWeaponDuty); 
}

void setup() {
  Serial.begin(115200);
  initalizeBluepad32(); 
  intializeMotors(); 
}

void loop() {
  BP32.update(); 

  if (millis() - lastMotorUpdate >= updateInterval) {
    lastMotorUpdate = millis();
    updateDrive();
    handleWeapon(); 
  }
}