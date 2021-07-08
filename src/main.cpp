#include <Arduino.h>
#include "MoodyAudio.h"

MoodyAudio<256> audio;

void setup() {
    Serial.begin(9600);
    audio.begin();
    audio.setSilenceThreshold();
}

void loop() {
    audio.listen();
    if(audio.ready()) {
        Serial.println(audio.analyze());
    }
}