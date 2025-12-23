#include <SPI.h>
#include <SD.h>
#include <TMRpcmSpeed32u4.h>

// Demo: RC PWM (1000..2000us) controls pitch/speed of a looping engine WAV.
// NOTE: uses pulseIn() (blocking) for simplicity.

TMRpcmSpeed32u4 audio;

const uint8_t SD_CS_PIN = 10;
const uint8_t RC_PIN    = 2;   // connect receiver PWM signal here (needs GND common)

void setup() {
  pinMode(RC_PIN, INPUT);

  // Timeout USB Serial: ne bloque pas si pas d'USB
  uint32_t t0 = millis();
  while (!Serial && (millis() - t0 < 1500)) {
    delay(1);
  }
  
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD init failed");
    while (1) {}
  }
  
  if (!audio.begin()) {
    Serial.println("Audio init failed");
    while (1) {}
  }
  
  audio.speakerPin = 6;   // mandatory on Pro Micro 32u4
  audio.setVolume(6);     // 0..7
  audio.loop(true);

  if (!audio.play("VAPEUR.IDL")) 
  {
    Serial.println("Play failed (check filename + WAV format)");
  }
}

void loop() {
  // read RC pulse width
  uint32_t us = pulseIn(RC_PIN, HIGH, 25000); // timeout 25ms
  if (us >= 900 && us <= 2100) {
    audio.setSpeedFromPulseUs((uint16_t)us, 1000, 2000, 0.70f, 1.80f);
  }

  audio.update();
}
