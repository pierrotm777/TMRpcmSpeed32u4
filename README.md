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
#include <TMRpcm.h>

TMRpcm audio;

void setup() {
  SD.begin(10);
  audio.speakerPin = 6;      // fixed on 32u4
  audio.setVolume(5);        // 0..7 (like TMRpcm style)
  audio.play("IDLE.WAV");
}

void loop() {
  audio.update();            // must be called often
}
```

## Speed / pitch control (TMRpcmSpeed-like)
- `audio.setPlaybackRate(rate);`  // 1.0 = normal, >1 faster/higher, <1 slower/lower
- `audio.speedUp();` / `audio.speedDown();`
- `audio.setSpeedFromPulseUs(us);` mapping 1000–2000 µs to a rate range

See `examples/rc_pwm_speed/rc_pwm_speed.ino`.

## Notes
This implementation uses:
- **Timer4** for PWM output on **D6 (OCR4D)**
- **Timer1** compare interrupt for sample timing (changed dynamically for speed control)

