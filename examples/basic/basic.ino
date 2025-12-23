#include <SPI.h>
#include <SD.h>
#include <TMRpcmSpeed32u4.h>

TMRpcmSpeed32u4 audio;

void setup() {
  Serial.begin(115200);
  // Timeout USB Serial: ne bloque pas si pas d'USB
  uint32_t t0 = millis();
  while (!Serial && (millis() - t0 < 1500)) {
    delay(1);
  }
  
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD init failed");
    while (1) {}
  }
  
  if(!audio.begin()) { // CS SD = D10 (adapt if needed)
    Serial.println("Audio init failed");
    while(1);
  }

  audio.setVolume(8); // 0..8
  if(!audio.play("1.WAV")) {
    Serial.println("play failed (need 8-bit mono PCM WAV)");
  }
}

void loop() {
  audio.update(); // important!
}
