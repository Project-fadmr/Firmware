void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(NINA_GPIO0, OUTPUT);
  pinMode(NINA_RESETN, OUTPUT);
  digitalWrite(NINA_GPIO0, HIGH);
  digitalWrite(NINA_RESETN, HIGH);
  delay(100);
  digitalWrite(NINA_RESETN, LOW);
  Serial1.begin(115200);
}

void loop() {
  if (Serial1.available() && Serial1.read() == 'B') {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println("blink");
  }
}
