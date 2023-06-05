#ifndef StepperDriver_h
#define StepperDriver_h

#include "ConfigHandler.h"
#include <Arduino.h>
#include <ezButton.h>

class StepperDriver {
public:
    // consctructor
    StepperDriver(short step, short dir, short enable);
    // methods for setting properties
    void begin(float rpm, ConfigHandler* configH, struct blindsConfig* config);
    void setButtons(ezButton* up, ezButton* down);
    // methods for moving motor in a desired way
    void movePercent(int percent);
    void moveSteps(long steps);
    void moveButtons(boolean manualMode);
    // control enablind/disabling of motor
    void enable(void);
    void disable(void);

private:
    // some field needed for work
    short stepPin;
    short dirPin;
    short enablePin;
    float rpm;
    short dirState;
    short activeState = LOW;
    short disableState = HIGH;
    short enabledState = 0; // 0 - disabled, 1 - enabled
    int delayTimeMicros;
    ConfigHandler* configHandler;
    ezButton* upButton;
    ezButton* downButton;
    struct blindsConfig* blindsConfig;
    // basic method for moving one step
    void oneStep();
};

#endif