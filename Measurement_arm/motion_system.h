#include <AccelStepper.h>
#include <TMC2209.h>
enum class Axis {
    YAW = 0,
    PITCH = 1
};

enum class CommandType {
    NONE = 0,
    MOVETO = 1,
    MOVEBY = 2,
    HOME = 3
};

struct Command
{
    CommandType commandType;
    Axis axis;
    int value;
};




class MotionSystem {
    public:
        MotionSystem(Stream& io, AccelStepper& yaw, TMC2209& yaw_driver, AccelStepper& pitch, TMC2209& pitch_driver)
        : io_(io), yaw_(yaw), yaw_driver_(yaw_driver), pitch_(pitch), pitch_driver_(pitch_driver) {}

        void run();
        void commandCenter();
        void homeAxes();
    private:
        bool homeSingleAxis(
        TMC2209 &driver,
        int runCurrent,
        int holdCurrent,
        int homingVelocity,
        int upperThreshold,
        int lowerThreshold,
        int repeatThreshold,
        unsigned long timeoutMs
        );
        AccelStepper* formatStepper(Command& command);
        void disableStealth(Command& command);
        int formatValue(Command& command);
        Command formatCommand(String command);
        void executeCommand(Command& command);
        void resetCurrents();

        
        void moveTo(int change, AccelStepper *stepper);
        void moveBy(int pos, AccelStepper *stepper);

        void sendAck();
        void sendAckRes(bool success);
        

        Stream& io_;
        TMC2209& yaw_driver_;
        TMC2209& pitch_driver_;
        AccelStepper& yaw_;
        AccelStepper& pitch_;
        bool in_motion_ = false;
};
