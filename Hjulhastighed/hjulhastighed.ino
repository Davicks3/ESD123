PulseSpeed speed(2, 4, 32.0f, 1.129f);

void setup() {
  Serial.begin(115200);
  speed.begin();
}

void loop() {
  auto d = speed.read();

  Serial.print(d.timeMs); Serial.print(" ms  ");
  Serial.print("rpmL="); Serial.print(d.rpmL, 2); Serial.print("  ");
  Serial.print("rpmR="); Serial.print(d.rpmR, 2); Serial.print("  ");
  Serial.print("cmL=");  Serial.print(d.cmL, 2);  Serial.print("  ");
  Serial.print("cmR=");  Serial.println(d.cmR, 2);

  delay(50);
}