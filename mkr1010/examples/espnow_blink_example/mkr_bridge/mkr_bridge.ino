void setup() {
  Serial.begin(115200);
  pinMode(NINA_GPIO0, OUTPUT);
  pinMode(NINA_RESETN, OUTPUT);
  digitalWrite(NINA_GPIO0, HIGH);
  digitalWrite(NINA_RESETN, HIGH);
  delay(100);
  digitalWrite(NINA_RESETN, LOW);
  Serial1.begin(115200);
}

void loop() {
  while (Serial1.available()) Serial.write(Serial1.read());
}
