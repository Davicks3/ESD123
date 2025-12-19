#include <Servo.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

//LiquidCrystal_I2C lcd(0x27,16,2);

Servo myservo;
int pos = 90;

int servoPin = 3;
int butPin = 10;
int gndPin = 8;
int trigPin = 7;

void setup() {
    pinMode(butPin, INPUT_PULLUP);
    pinMode(gndPin, OUTPUT);
    pinMode(trigPin, OUTPUT);
    digitalWrite(trigPin, LOW);
    digitalWrite(gndPin, LOW);

    myservo.attach(servoPin);
    myservo.write(pos);

    //lcd.init();
    //lcd.backlight();
    //lcd.setCursor(0,0);
    //lcd.print(90-pos);
    //lcd.print(" Grader");

}

void loop() {
    if (!digitalRead(butPin)) {
        digitalWrite(trigPin, HIGH);
        pos -= 35;
        pos = max(pos, 0);
        myservo.write(pos);
        while (true) {}
        //lcd.setCursor(0,0);
        //lcd.print(90-pos);
        //lcd.print(" Grader");
        delay(100); //Avoid double clicking :)
        while(!digitalRead(butPin)) { delay(10); }
        delay(100);
    }

}
