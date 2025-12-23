#include "TMRpcmSpeed32u4.h"
#include <string.h>
#include <avr/interrupt.h>

#if !defined(__AVR_ATmega32U4__)
  #error "TMRpcmSpeed32u4 is intended for ATmega32u4 boards (Pro Micro / Micro / Leonardo)."
#endif

// -------- Static members --------
TMRpcmSpeed32u4* TMRpcmSpeed32u4::_active = nullptr;
volatile uint8_t  TMRpcmSpeed32u4::_buf[TMRpcmSpeed32u4::BUF_SZ];
volatile uint16_t TMRpcmSpeed32u4::_rd = 0;
volatile uint16_t TMRpcmSpeed32u4::_wr = 0;
volatile uint16_t TMRpcmSpeed32u4::_count = 0;

// Optional ISR counter (useful for debugging)
volatile uint32_t g_isrCount = 0;

ISR(TIMER1_COMPA_vect) {
  g_isrCount++;
  TMRpcmSpeed32u4::_isrService();
}

TMRpcmSpeed32u4::TMRpcmSpeed32u4() {}

bool TMRpcmSpeed32u4::begin(uint8_t sdCsPin) {
  _csPin = sdCsPin;

  _playing = false;
  _paused  = false;
  _active  = nullptr;

  noInterrupts();
  _rd = _wr = _count = 0;
  interrupts();

  _setupPwmTimer4();
  OCR4D = 127;
  return true;
}

void TMRpcmSpeed32u4::setVolume(uint8_t v) {
  if (v > 8) v = 8;
  // v=8 => loudest => shift 0; v=0 => quiet => shift 4 (approx)
  _volumeShift = (uint8_t)((8 - v) / 2); // 0..4
}

bool TMRpcmSpeed32u4::play(const char* filename) {
  stop();
  _paused = false;

  if (filename) {
    strncpy(_currentName, filename, sizeof(_currentName) - 1);
    _currentName[sizeof(_currentName) - 1] = '\0';
  } else {
    _currentName[0] = '\0';
  }

  if (!_openAndParseWav(filename)) {
    return false;
  }

  noInterrupts();
  _rd = _wr = _count = 0;
  interrupts();

  _active = this;
  _playing = true;

  // Prime buffer a bit
  update();
  update();

  // Force Timer1 setup (robust on 32u4)
  _forceTimer1Ctc(_wavSampleRate);
  _applyRateToTimer1();

  // Enable Timer1 compare A IRQ
  noInterrupts();
  TIFR1  = (1 << OCF1A);
  TIMSK1 |= (1 << OCIE1A);
  interrupts();

  return true;
}

void TMRpcmSpeed32u4::stop() {
  noInterrupts();
  TIMSK1 &= ~(1 << OCIE1A);
  interrupts();

  _playing = false;
  _paused  = false;
  _active  = nullptr;

  if (_file) _file.close();

  OCR4D = 127;
}

void TMRpcmSpeed32u4::pause(bool muteOutput) {
  if (!_playing || _paused) return;

  cli();
  _paused = true;
  TIMSK1 &= ~(1 << OCIE1A); // stop Timer1 tick
  if (muteOutput) OCR4D = 127;
  sei();
}

void TMRpcmSpeed32u4::resume() {
  if (!_playing || !_paused) return;

  cli();
  _paused = false;
  TIFR1  = (1 << OCF1A);
  TIMSK1 |= (1 << OCIE1A);
  sei();
}

void TMRpcmSpeed32u4::update() {
  if (!_playing || _paused || !_file) return;

  // --- Critical fix: keep file cursor synchronized ---
  uint32_t want = _dataStart + _dataPos;
  if ((uint32_t)_file.position() != want) {
    _file.seek(want);
  }

  // Fill buffer while there is space and remaining data
  while (_count < (BUF_SZ - 32) && _dataPos < _dataSize) {
    uint16_t space = (uint16_t)(BUF_SZ - _count);
    if (!space) break;

    uint16_t toEnd = (uint16_t)(BUF_SZ - _wr);
    uint16_t n = (space < toEnd) ? space : toEnd;

    // Small read chunk to reduce blocking
    if (n > 32) n = 32;

    uint32_t rem = _dataSize - _dataPos;
    if (n > rem) n = (uint16_t)rem;
    if (!n) break;

    uint8_t tmp[32];
    int r = _file.read(tmp, n);
    if (r <= 0) {
      // read failed or EOF unexpectedly; exit to avoid spinning
      break;
    }

    noInterrupts();
    for (int i = 0; i < r; i++) {
      _buf[_wr] = tmp[i];
      _wr++;
      if (_wr >= BUF_SZ) _wr = 0;
      _count++;
    }
    interrupts();

    _dataPos += (uint32_t)r;
  }

  // End reached and buffer empty => loop or stop
  if (_dataPos >= _dataSize && _count == 0) {
    if (loopPlayback && _currentName[0] != '\0') {
      stop();
      play(_currentName);
    } else {
      stop();
    }
  }
}

// ISR service: outputs next sample to OCR4D
void TMRpcmSpeed32u4::_isrService() {
  TMRpcmSpeed32u4* self = _active;
  if (!self || !self->_playing || self->_paused) {
    OCR4D = 127;
    return;
  }

  uint8_t s = 127;

  if (_count) {
    s = _buf[_rd];
    _rd++;
    if (_rd >= BUF_SZ) _rd = 0;
    _count--;
  } else {
    s = 127; // underrun => silence
  }

  // Volume attenuation by shifting toward midpoint
  if (self->_volumeShift) {
    int16_t centered = (int16_t)s - 128;
    centered >>= self->_volumeShift;
    s = (uint8_t)(centered + 128);
  }

  OCR4D = s;
}

// ---------- WAV parsing helpers ----------
static uint32_t readLE32(File &f) {
  uint8_t b[4];
  if (f.read(b,4) != 4) return 0;
  return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}
static uint16_t readLE16(File &f) {
  uint8_t b[2];
  if (f.read(b,2) != 2) return 0;
  return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

bool TMRpcmSpeed32u4::_openAndParseWav(const char* filename) {
  _file = SD.open(filename, FILE_READ);
  if (!_file) return false;

  char riff[4];
  char wave[4];
  if (_file.read((uint8_t*)riff, 4) != 4) { _file.close(); return false; }
  (void)readLE32(_file);
  if (_file.read((uint8_t*)wave, 4) != 4) { _file.close(); return false; }

  if (memcmp(riff, "RIFF", 4) != 0 || memcmp(wave, "WAVE", 4) != 0) {
    _file.close();
    return false;
  }

  bool gotFmt = false, gotData = false;
  uint16_t audioFormat = 0, numCh = 0, bits = 0;
  uint32_t sr = 0;
  uint32_t dataStart = 0, dataSize = 0;

  while (_file.available()) {
    char id[4];
    if (_file.read((uint8_t*)id, 4) != 4) break;
    uint32_t sz = readLE32(_file);

    if (memcmp(id, "fmt ", 4) == 0) {
      audioFormat = readLE16(_file);
      numCh       = readLE16(_file);
      sr          = readLE32(_file);
      (void)readLE32(_file);       // byteRate
      (void)readLE16(_file);       // blockAlign
      bits        = readLE16(_file);

      // Skip any fmt extension
      if (sz > 16) _skip(sz - 16);
      gotFmt = true;
    }
    else if (memcmp(id, "data", 4) == 0) {
      // --- Critical fix: DO NOT skip the data chunk here ---
      dataStart = (uint32_t)_file.position();
      dataSize  = sz;
      gotData = true;
      break; // we have what we need
    }
    else {
      _skip(sz);
    }
  }

  if (!gotFmt || !gotData) { _file.close(); return false; }
  if (audioFormat != 1)    { _file.close(); return false; } // PCM
  if (numCh != 1)          { _file.close(); return false; } // mono only
  if (bits != 8)           { _file.close(); return false; } // 8-bit only

  _wavSampleRate = (sr ? sr : 8000);
  _dataStart = dataStart;
  _dataSize  = dataSize;
  _dataPos   = 0;

  _file.seek(_dataStart); // start of PCM samples
  return true;
}

void TMRpcmSpeed32u4::_skip(uint32_t n) {
  while (n--) {
    if (_file.read() < 0) break;
  }
}

// ---------- Timers ----------
void TMRpcmSpeed32u4::_setupPwmTimer4() {
  pinMode(6, OUTPUT);

  // --- IMPORTANT: do not clobber PLLCSR (USB uses PLL) ---
  uint8_t pll = PLLCSR;

  // Ensure PLL enabled
  if (!(pll & (1 << PLLE))) {
    PLLCSR = pll | (1 << PLLE);
    while (!(PLLCSR & (1 << PLOCK))) { }
  }

  // Enable Timer4 clock from PLL (preserve other bits)
  PLLCSR |= (1 << PLLTM0);

  // Reset Timer4
  TCCR4A = 0;
  TCCR4B = 0;
  TCCR4C = 0;
  TCCR4D = 0;
  TCCR4E = 0;
  TCNT4  = 0;

  // Fast PWM on OC4D (D6)
  TCCR4C |= (1 << COM4D1);
  TCCR4C |= (1 << PWM4D);

  // Prescaler /1
  TCCR4B |= (1 << CS40);

  OCR4D = 127;
}


void TMRpcmSpeed32u4::_forceTimer1Ctc(uint32_t sampleRate) {
  if (sampleRate < 4000)  sampleRate = 4000;
  if (sampleRate > 32000) sampleRate = 32000;

  uint32_t ocr = (F_CPU / sampleRate) - 1;
  if (ocr > 65535) ocr = 65535;
  if (ocr < 50)    ocr = 50;

  _baseOcr1a = (uint16_t)ocr;

  noInterrupts();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;
  OCR1A  = (uint16_t)ocr;
  TCCR1B = (1 << WGM12) | (1 << CS10); // CTC, /1
  interrupts();
}

void TMRpcmSpeed32u4::setPlaybackRate(float rate) {
  if (rate < 0.3f) rate = 0.3f;
  if (rate > 3.0f) rate = 3.0f;
  _playbackRate = rate;
  _applyRateToTimer1();
}

void TMRpcmSpeed32u4::setSpeedFromPulseUs(uint16_t us,
                                         uint16_t inMinUs, uint16_t inMaxUs,
                                         float rateMin, float rateMax)
{
  if (inMinUs >= inMaxUs) return;
  if (us < inMinUs) us = inMinUs;
  if (us > inMaxUs) us = inMaxUs;

  float t = (float)(us - inMinUs) / (float)(inMaxUs - inMinUs);
  float r = rateMin + t * (rateMax - rateMin);
  setPlaybackRate(r);
}

void TMRpcmSpeed32u4::_applyRateToTimer1() {
  if (!_baseOcr1a) return;

  float rate = _playbackRate;
  if (rate < 0.3f) rate = 0.3f;
  if (rate > 3.0f) rate = 3.0f;

  uint32_t basePlus1 = (uint32_t)_baseOcr1a + 1u;
  uint32_t ocr = (uint32_t)((float)basePlus1 / rate);
  if (ocr == 0) ocr = 1;
  ocr -= 1;

  if (ocr > 65535u) ocr = 65535u;
  if (ocr < 50u)    ocr = 50u;

  noInterrupts();
  OCR1A = (uint16_t)ocr;
  interrupts();
}
