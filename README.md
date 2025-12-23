## TMRpcmSpeed32u4 v1.0.2

# TMRpcmSpeed32u4 v1.0.0

API and “philosophy” compatible with **TMRpcm / TMRpcmSpeed**, but adapted for **ATmega32u4** (Arduino **Pro Micro / Micro / Leonardo**) with audio on **D6**.

## Hardware / pins
- **Audio out:** D6 (OC4D / Timer4) **mandatory**
- **SD CS:** configurable (examples use D10)
- SPI uses ICSP pins on 32u4 boards.

## WAV format supported
- **PCM unsigned 8-bit**
- **Mono**
- Recommended sample rate: **8–16 kHz** (works up to ~22 kHz)

## Basic example
```cpp
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
```

## Speed / pitch control (TMRpcmSpeed-like)
- `audio.setPlaybackRate(rate);`  // 1.0 = normal, >1 faster/higher, <1 slower/lower
- `audio.setSpeedFromPulseUs(us);` mapping 1000–2000 µs to a rate range

See `examples/rc_pwm_speed/rc_pwm_speed.ino`.

## Notes
This implementation uses:
- **Timer4** for PWM output on **D6 (OCR4D)**
- **Timer1** compare interrupt for sample timing (changed dynamically for speed control)

