#include "motion_system.h"
#include "settings.h"


void MotionSystem::run() {
    this->yaw_.run();
    this->pitch_.run();

    if (!in_motion_) { return; }

    if (pitch_.distanceToGo() == 0 and yaw_.distanceToGo() == 0) {
      this->yaw_driver_.enableStealthChop();
      this->pitch_driver_.enableStealthChop();
      this->sendAckRes(true);
      in_motion_ = false;
    }
}

void MotionSystem::commandCenter() {
    if (!this->io_.available()) { return; }

    String command = io_.readStringUntil('\n');
    command.trim();
    if (command.length() == 0) return;
    this->sendAck();
    // Expected:
    // _MT_%axis_%pos_
    // _MB_%axis_%delta_
    // _HOME_
    Command toDo = this->formatCommand(command);
    if (toDo.commandType == CommandType::NONE) { this->sendAckRes(false); return; }

    this->executeCommand(toDo);
}

Command MotionSystem::formatCommand(String command) {
    Command noneCommand;
    noneCommand.commandType = CommandType::NONE;
    
    Command result;
    result.commandType = CommandType::NONE;

    for (int i = 0; i < command.length(); i++) {
        
        if (command[i] != '_') { continue; }
        if (this->in_motion_) { return noneCommand; }

        command = command.substring(i+1);
        if (command.startsWith("MT_")) { result.commandType = CommandType::MOVETO; }
        else if (command.startsWith("MB_")) { result.commandType = CommandType::MOVEBY; }
        else if (command.startsWith("HOME_")) { result.commandType = CommandType::HOME; return result;}
        else { return result; }
        command = command.substring(3);

        if (command[0] == 'Y') { result.axis = Axis::YAW; } else { result.axis = Axis::PITCH; }

        for (int j = 2; j < command.length(); j++) {
            if (command[j] == '_') { command = command.substring(2, j); break; }
            if (j >= command.length()) { return noneCommand; }
        }
        result.value = command.toInt();
        return result;
    }
    return noneCommand;
}

void MotionSystem::disableStealth(Command& command) {
    switch (command.axis) {
            case Axis::YAW:
                this->yaw_driver_.disableStealthChop();
                break;
            case Axis::PITCH:
                this->pitch_driver_.disableStealthChop();
                break;
            default:
                return;
        }
}

AccelStepper* MotionSystem::formatStepper(Command& command) {
    switch (command.axis) {
        case Axis::YAW:
            return &yaw_;
        case Axis::PITCH:
            return &pitch_;
        default:
            return nullptr;
    }
}

int MotionSystem::formatValue(Command& command) {
  switch (command.axis) {
        case Axis::YAW:
            return int(command.value * STEPS_PER_DEGREE_YAW * MICROSTEPS_YAW * DIRECTION_YAW);
        case Axis::PITCH:
            return int(command.value * STEPS_PER_DEGREE_PITCH * MICROSTEPS_PITCH * DIRECTION_PITCH);
        default:
            return 0;
    }
}

void MotionSystem::resetCurrents() {
    this->yaw_driver_.setRunCurrent(RUN_CURRENT_PERCENT_YAW); delay(10);
    this->pitch_driver_.setRunCurrent(RUN_CURRENT_PERCENT_PITCH); delay(10);

    this->yaw_driver_.setHoldCurrent(HOLD_CURRENT_PERCENT_YAW); delay(10);
    this->pitch_driver_.setHoldCurrent(HOLD_CURRENT_PERCENT_PITCH); delay(10);

    this->yaw_driver_.setStallGuardThreshold(255); delay(10);
    this->pitch_driver_.setStallGuardThreshold(255); delay(10);

    this->yaw_driver_.setMicrostepsPerStep(MICROSTEPS_YAW); delay(10);
    this->pitch_driver_.setMicrostepsPerStep(MICROSTEPS_PITCH); delay(10);
    
}

void MotionSystem::executeCommand(Command& command) {
    
    switch (command.commandType)
    {
    case CommandType::NONE:
        this->sendAckRes(false);
        break;
    case CommandType::HOME:
        this->homeAxes();
        break;
    case CommandType::MOVEBY:
        this->disableStealth(command);
        this->moveBy(this->formatValue(command), this->formatStepper(command));
        this->in_motion_ = true;
        break;
    case CommandType::MOVETO:
        this->disableStealth(command);
        this->moveTo(this->formatValue(command), this->formatStepper(command));
        this->in_motion_ = true;
        break;
    
    default:
        break;
    }
}

bool MotionSystem::homeSingleAxis(
    TMC2209 &driver,
    int runCurrent,
    int holdCurrent,
    int homingVelocity,
    int upperThreshold,
    int lowerThreshold,
    int repeatThreshold,
    unsigned long timeoutMs
) {
      driver.setRunCurrent(runCurrent); delay(10);
      driver.setHoldCurrent(holdCurrent); delay(10);
      driver.disableStealthChop(); delay(10);
      //driver.setMicrostepsPerStep(1); delay(10);
      driver.moveAtVelocity(homingVelocity); delay(10);
  
      bool homed = false;
      unsigned int sg_count = 0;
      unsigned long t0 = millis() + HOMING_INITIAL_WAIT_MS;

      delay(HOMING_INITIAL_WAIT_MS);
      
      while (!homed) {
          delay(1);
          uint16_t sg = driver.getStallGuardResult();
  
          if (sg <= upperThreshold && sg >= lowerThreshold) {
              sg_count++;
          }
  
          if (!homed && sg_count >= repeatThreshold) {
              driver.moveAtVelocity(0); delay(10);
              driver.enableStealthChop(); delay(10);
              homed = true;
          }
  
          unsigned long delta = millis() - t0;
          if (!homed && delta >= timeoutMs) {
              driver.moveAtVelocity(0); delay(10);
              driver.enableStealthChop(); delay(10);
              return false; // fail
          }
      }
  
      return true;
}

void MotionSystem::homeAxes() {
    this->yaw_.stop();
    this->pitch_.stop();

    // Yaw first
    bool yaw_ok = homeSingleAxis(
        this->yaw_driver_,
        HOMING_RUN_CURRENT_PERCENT_YAW,
        HOMING_RUN_CURRENT_PERCENT_YAW,
        HOMING_VELOCITY_YAW * DIRECTION_YAW,
        STALL_GUARD_UPPER_THRESHOLD_YAW,
        STALL_GUARD_LOWER_THRESHOLD_YAW,
        HOMING_STALL_REPEAT_THRESHOLD_YAW,
        HOMING_TIMEOUT_MS_YAW
    );
    this->resetCurrents();

    
    // Pitch second
    bool pitch_ok = homeSingleAxis(
        this->pitch_driver_,
        HOMING_RUN_CURRENT_PERCENT_PITCH,
        HOMING_RUN_CURRENT_PERCENT_PITCH,
        HOMING_VELOCITY_PITCH * DIRECTION_PITCH,
        STALL_GUARD_UPPER_THRESHOLD_PITCH,
        STALL_GUARD_LOWER_THRESHOLD_PITCH,
        HOMING_STALL_REPEAT_THRESHOLD_PITCH,
        HOMING_TIMEOUT_MS_PITCH
    );
    this->resetCurrents();
    
    if (!yaw_ok || !pitch_ok) {
        this->sendAckRes(false);
        return;
    }

    delay(HOMING_SETTLE_MS);

    this->yaw_.setCurrentPosition(int(float(YAW_OFFSET)   * float(DIRECTION_YAW)   * float(STEPS_PER_DEGREE_YAW)   * float(MICROSTEPS_YAW)));
    this->pitch_.setCurrentPosition(int(float(PITCH_OFFSET) * float(DIRECTION_PITCH) * float(STEPS_PER_DEGREE_PITCH) * float(MICROSTEPS_PITCH)));

    this->in_motion_ = false;
    this->sendAckRes(true);
}

void MotionSystem::moveBy(int change, AccelStepper* stepper) {
    // Not recomended... Will accumulate rounding errors. Didn't fix it :)
    this->yaw_driver_.disableStealthChop();
    this->pitch_driver_.disableStealthChop();
    stepper->move(change);
}

void MotionSystem::moveTo(int pos, AccelStepper* stepper) {
    stepper->moveTo(pos);
}

void MotionSystem::sendAckRes(bool success) {
    io_.print(success ? "_ACKOK\n" : "_ACKFAIL\n");
}

void MotionSystem::sendAck() {
    io_.print("_ACK\n");
}
