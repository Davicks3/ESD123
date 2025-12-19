#include <TMC2209.h>
#include <AccelStepper.h>
#include "settings.h"
#include "motion_system.h"


/*
This will be able to take commands over serial, to move the arm.
An acknowledgement will be sent after completion of given command.
Axes:
- "0": yaw
- "1": pitch
Commands:
- "_MT_%axis_%pos": move (Y/P) axis to (int) pos
- "_MB_%axis_%pos change": move (int) axis by (int) pos change
- "_HOME": home both axes
Acknowledgement:
- "_ACKOK": OK
- "_ACKFAIL": Fail
*/
TMC2209 stepper_driver_yaw;
TMC2209 stepper_driver_pitch;

AccelStepper stepper_yaw(AccelStepper::DRIVER, STEP_PIN_YAW, DIR_PIN_YAW);
AccelStepper stepper_pitch(AccelStepper::DRIVER, STEP_PIN_PITCH, DIR_PIN_PITCH);

#include "setup.h"

MotionSystem motion_system(Serial, stepper_yaw, stepper_driver_yaw, stepper_pitch, stepper_driver_pitch);

void setup() {
    Serial.begin(115200);
    setup_steppers();
    //motion_system.homeAxes();
}

void loop() {
    motion_system.run();
    motion_system.commandCenter();
}
