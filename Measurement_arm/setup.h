
HardwareSerial & serialYaw = Serial1;
HardwareSerial & serialPitch = Serial2;


void setup_steppers()
{
    Serial1.begin(SERIAL_BAUD_RATE, SERIAL_8N1, RX_PITCH, TX_PITCH);
    Serial2.begin(SERIAL_BAUD_RATE, SERIAL_8N1, RX_YAW, TX_YAW);


    stepper_driver_yaw.setup(serialYaw, SERIAL_BAUD_RATE, TMC2209::SERIAL_ADDRESS_0, RX_YAW, TX_YAW); delay(10);
    stepper_driver_pitch.setup(serialPitch, SERIAL_BAUD_RATE, TMC2209::SERIAL_ADDRESS_0, RX_PITCH, TX_PITCH); delay(10);

    stepper_driver_yaw.setRunCurrent(RUN_CURRENT_PERCENT_YAW); delay(10);
    stepper_driver_pitch.setRunCurrent(RUN_CURRENT_PERCENT_PITCH); delay(10);
    
    stepper_driver_yaw.setHoldCurrent(HOLD_CURRENT_PERCENT_YAW); delay(10);
    stepper_driver_pitch.setHoldCurrent(HOLD_CURRENT_PERCENT_PITCH); delay(10);

    stepper_driver_yaw.setMicrostepsPerStep(MICROSTEPS_YAW); delay(10);
    stepper_driver_pitch.setMicrostepsPerStep(MICROSTEPS_PITCH); delay(10);

    stepper_driver_yaw.enableStealthChop(); delay(10);
    stepper_driver_pitch.enableStealthChop(); delay(10);

    stepper_yaw.setMaxSpeed(MAX_VELOCITY_YAW * MICROSTEPS_YAW);
    stepper_pitch.setMaxSpeed(MAX_VELOCITY_PITCH * MICROSTEPS_PITCH);

    stepper_yaw.setAcceleration(ACCEL_YAW * MICROSTEPS_YAW);
    stepper_pitch.setAcceleration(ACCEL_PITCH * MICROSTEPS_PITCH);

    stepper_driver_yaw.enable(); delay(10);
    stepper_driver_pitch.enable(); delay(10);
}
