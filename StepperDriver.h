#ifndef StepperDriver_h
#define StepperDriver_h

class StepperDriver {
public:
    StepperDriver(short step, short dir, short enable);

    void begin(float rpm);
    void setRpm(float rpm);
    float getRpm(void);

    void move(long steps);

    void enable(void);
    void disable(void);

    enum State getCurrentState(void);

    long getStepsRemaining(void)

        private :

        enum State {
            MOVING,
            STOPPED
        };
    short stepPin;
    short dirPin;
    short enablePin;
    float RPM;
    short dirState;
    short activeState = LOW;
    , static const int stepHighMin = 1;
    static const int stepLowMin = 1;
    static const int wakeup_time = 0;
}

#endif