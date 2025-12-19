#include <Arduino.h>

class PID {
    public:
        float Kp, Ki, Kd;
        float alpha = 0.3;
        PID(float Kp, float Ki, float Kd) : Kp(Kp), Ki(Ki), Kd(Kd) {}

        void setKp(float Kp) { this->Kp = Kp; }
        void setKi(float Ki) { this->Ki = Ki; }
        void setKd(float Kd) { this->Kd = Kd; }
        void setAlpha(float alpha) { this->alpha = alpha; }

        float calcOutput(float ref, float feedback) {
            float dt = this->calcDt();
            float error = ref - feedback;
            float output = 0.0;
            output += this->calcProportional(error);
            output += this->calcIntegral(error, dt);
            output += this->calcDerivative(error, dt);
            return output;
        }
    
    private:
        float calcProportional(float error) {
            return this->Kp * error;
        }

        float calcIntegral(float error, float dt) {
            static float integral = 0.0;
            integral += (dt * error);
            return this->Ki * integral;
        }

        float calcDerivative(float error, float dt) {
            static float prevError = error;
            float derivativeRaw = (error - prevError) / dt;
            prevError = error;
            float filteredDerivative =  filteredDerivative + this->alpha * (derivativeRaw - filteredDerivative); // et første ordens lavpass filter som reducere støj eller andre voldsome ændringer
            return this->Kd * filteredDerivative; 
        }

        float calcDt() {
            static unsigned long prevTime = millis();
            unsigned long delta =  millis() - prevTime;
            float dt = float(delta) / 1000.0;
            prevTime += delta;
            return dt;
        }

};
