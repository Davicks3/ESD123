// Comunication
// ########################
const long SERIAL_BAUD_RATE = 115200;
const int RX_YAW = 18;
const int TX_YAW = 5;
const int RX_PITCH = 16;
const int TX_PITCH = 17;

// Homing
// ########################
#define HOMING_TIMEOUT_MS_YAW 20000
#define HOMING_TIMEOUT_MS_PITCH 300000
#define HOMING_SETTLE_MS 500
#define STALL_GUARD_LOWER_THRESHOLD_YAW 40
#define STALL_GUARD_UPPER_THRESHOLD_YAW 250
#define STALL_GUARD_LOWER_THRESHOLD_PITCH 12
#define STALL_GUARD_UPPER_THRESHOLD_PITCH 40
#define HOMING_RUN_CURRENT_PERCENT_YAW 7
#define HOMING_RUN_CURRENT_PERCENT_PITCH 10
#define HOMING_STALL_REPEAT_THRESHOLD_YAW 5
#define HOMING_STALL_REPEAT_THRESHOLD_PITCH 5
#define HOMING_INITIAL_WAIT_MS 20

// Offsets
// ########################
#define YAW_OFFSET -90 // Degrees
#define PITCH_OFFSET -81 // Degrees

// Currents
// ########################
#define RUN_CURRENT_PERCENT_YAW 60
#define HOLD_CURRENT_PERCENT_YAW 2
#define RUN_CURRENT_PERCENT_PITCH 80
#define HOLD_CURRENT_PERCENT_PITCH 2

// Stepping
// ########################
#define MICROSTEPS_YAW 1
#define MICROSTEPS_PITCH 1
#define STEPS_PER_DEGREE_YAW 62.5
#define STEPS_PER_DEGREE_PITCH 650

// Movement
// ########################
#define DIRECTION_YAW -1
#define DIRECTION_PITCH -1
#define HOMING_VELOCITY_YAW 800
#define HOMING_VELOCITY_PITCH 600
#define MAX_VELOCITY_YAW 800
#define MAX_VELOCITY_PITCH 600
#define ACCEL_YAW 200
#define ACCEL_PITCH 400

// Pins
// ########################
#define STEP_PIN_YAW 23
#define STEP_PIN_PITCH 4
#define DIR_PIN_YAW 22
#define DIR_PIN_PITCH 2
