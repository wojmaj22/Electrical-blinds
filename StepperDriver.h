#ifndef StepperDriver_h
#define StepperDriver_h

class StepperDriver {
public:
    StepperDriver(short step, short dir, short enable);

private:
    short stepPin;
    short dirPin;
    short enablePin;
    int RPM;
    short dirState;
}

#endif