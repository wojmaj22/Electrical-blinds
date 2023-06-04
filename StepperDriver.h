#ifndef StepperDriver_h
#define StepperDriver_h

#include "ConfigHandler.h"
#include <Arduino.h>
#include <ezButton.h>

class StepperDriver {
public:
    StepperDriver(short step, short dir, short enable);

    void begin(float rpm, ConfigHandler* configH, struct blindsConfig* config);
    void setButtons(ezButton* up, ezButton* down);

    void movePercent(int percent);
    void moveSteps(long steps);
    int moveButtons(boolean manualMode);

    void enable(void);
    void disable(void);

private:
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
    struct blindsConfig *blindsConfig;

    void oneStep();
};

#endif