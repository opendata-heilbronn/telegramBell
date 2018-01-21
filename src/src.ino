#include <Arduino.h>

#define BELL_PIN D0

int hertz = 30;
int pulseRatio = 4; //on-time divider of transistor 

void setup() {
    Serial.begin(115200);
    pinMode(BELL_PIN, OUTPUT);
    digitalWrite(BELL_PIN, LOW);
}

unsigned long now, lastPulse = 0;

void loop() {
    now = millis();

    if(now - lastPulse >= (1000 / hertz)) {
        lastPulse = now;
        digitalWrite(BELL_PIN, HIGH);
    }
    if(now - lastPulse >= (1000 / hertz) / pulseRatio) {
        digitalWrite(BELL_PIN, LOW);
    }



    if(Serial.available()) {
        hertz = Serial.parseInt();
    }
}