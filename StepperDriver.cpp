#include "StepperDriver.h"
// TODO - na koniec usunąć stąd Serial.print
StepperDriver::StepperDriver(short step, short dir, short enable)
{
    // sets step, dir, and enable pins
    stepPin = step;
    dirPin = dir;
    enablePin = enable;
}

void StepperDriver::begin(float rpm, ConfigHandler* configH, struct blindsConfig* config)
{
    // saves rpm and calculates delay times between switching step output, sets pins to output mode
    this->rpm = rpm;
    pinMode(stepPin, OUTPUT);
    pinMode(dirPin, OUTPUT);
    pinMode(enablePin, OUTPUT);

    int delayTimeMicros = 60 * 1000000 / (200 * rpm * 2);
    this->delayTimeMicros = delayTimeMicros;
    Serial.print("TEST");
    Serial.printf(", delay time is %i microseconds. \n", this->delayTimeMicros);
    configHandler = configH;
    blindsConfig = config;
    Serial.println("Reading config...");
    //configHandler->readBlindsConfig(blindsConfig);
    Serial.printf("Position: %i, max: %i \n", blindsConfig->currentPosition, blindsConfig->maxSteps);

    StepperDriver::disable();
}

// sets buttons and their pins to output
void StepperDriver::setButtons(ezButton* up, ezButton* down)
{
    
    upButton = up;
    downButton = down;
    up->setDebounceTime(50);
    down->setDebounceTime(50);
}

// converts percent of range to steps and moves motor
void StepperDriver::movePercent(int percent)
{
    double onePercentSteps = blindsConfig->maxSteps / 100.0;
    int stepsInt = 0;
    double stepsNeeded = 0;
    if (percent == 0) {
        stepsInt = -1 * blindsConfig->currentPosition;
    } else if (percent == 100) {
        stepsInt = blindsConfig->maxSteps - blindsConfig->currentPosition;
    } else if (percent > 0 && percent < 100) {
        stepsNeeded = (percent * onePercentSteps) - blindsConfig->currentPosition;
        stepsInt = (int)stepsNeeded;
    }
    Serial.printf("%.2f - oPS, %i - current, %i - max, %.2f - steps needed \n", onePercentSteps, blindsConfig->currentPosition, blindsConfig->maxSteps, stepsNeeded);
    Serial.printf("Moving %i steps \n", stepsInt);
    StepperDriver::moveSteps(stepsInt);
}

// enables motor, then moves according to given steps
// after movement disables stepper and saves changed position to memory
void StepperDriver::moveSteps(long steps)
{
    StepperDriver::enable();

    if (steps > 0) {
        Serial.println("Moving down");
        digitalWrite(dirPin, HIGH);
    } else if (steps < 0) {
        Serial.println("Moving up");
        digitalWrite(dirPin, LOW);
    } else{
        return;
    }
    int n = abs(steps);
    for (int i = 0; i < n; i++) {
        oneStep();
    }
    blindsConfig->currentPosition += steps;
    StepperDriver::disable();
    Serial.printf("Moved %i steps \n", steps);
    configHandler->saveBlindsConfig(*blindsConfig);
}

// moves motor with button control, while button is clicked motor is running in desired direction
int StepperDriver::moveButtons(boolean manualMode)
{
    int count = 0;
    downButton->loop();
    upButton->loop();
    if (downButton->getState() == HIGH && (manualMode || blindsConfig->currentPosition < blindsConfig->maxSteps)) {
        Serial.printf("Moving down with buttons. \n");
        StepperDriver::enable();
        digitalWrite(dirPin, HIGH);
        downButton->loop();
        while (downButton->getState() == HIGH && (manualMode || blindsConfig->currentPosition < blindsConfig->maxSteps)) {
            oneStep();
            count++;
            blindsConfig->currentPosition++;
            downButton->loop();
        }
        Serial.printf("Moved down %i steps with buttons. \n", count);
        StepperDriver::disable();
        configHandler->saveBlindsConfig(*blindsConfig);

    } else if (upButton->getState() == HIGH && (manualMode || blindsConfig->currentPosition > 0)) {
        Serial.printf("Moving up with buttons. \n");
        StepperDriver::enable();
        digitalWrite(dirPin, LOW);
        upButton->loop();
        while (upButton->getState() == HIGH && (manualMode || blindsConfig->currentPosition > 0)) {
            oneStep();
            count--;
            blindsConfig->currentPosition--;
            upButton->loop();
        }
        Serial.printf("Moved up %i steps with buttons. \n", count);
        StepperDriver::disable();
        configHandler->saveBlindsConfig(*blindsConfig);
    }
    return count;
}

// moves motor by one step
void StepperDriver::oneStep(){
  digitalWrite(stepPin, HIGH);
  delayMicroseconds(delayTimeMicros);
  yield();
  digitalWrite(stepPin, LOW);
  delayMicroseconds(delayTimeMicros);
  yield();
}

// enables motor with enable pin
void StepperDriver::enable()
{
    delayMicroseconds(500);
    digitalWrite(enablePin, activeState);
    enabledState = 1;
    delayMicroseconds(10);
    Serial.println("Stepper enabled");
}

// disables motor with enable pin
void StepperDriver::disable()
{
    delayMicroseconds(500);
    digitalWrite(enablePin, disableState);
    enabledState = 0;
    delayMicroseconds(10);
    Serial.println("Stepper disabled");
}
