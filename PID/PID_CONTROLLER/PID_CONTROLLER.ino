#include "PID.cpp"


PID controller(0.1, 0.2, 0.3);

void setup() {
    controller.setAlpha(0.5);
}


void loop() {
    controller.calcOutput(0.0, 10.0);

}
