#pragma once
#include <Arduino.h>
#include <SD.h>

class TMRpcmSpeed32u4 {
public:
  TMRpcmSpeed32u4();

  // API style TMRpcm
  uint8_t speakerPin = 6;      // fixed to D6 (OC4D) on ATmega32u4
  bool    loopPlayback = false;

  // Important: this begin() DOES NOT call SD.begin().
  // You must call SD.begin(CS) in your sketch first.
  bool begin(uint8_t sdCsPin = 10);

  bool play(const char* filename);
  void stop();
  void stopPlayback() { stop(); }

  bool isPlaying() const { return _playing; }

  // Pause/resume for exclusive SD access
  void pause(bool muteOutput = true);
  void resume();
  bool isPaused() const { return _paused; }

  // Non-blocking buffering, call often
  void update();

  // Volume 0..8 (8 = max)
  void setVolume(uint8_t v);

  // Speed / pitch control
  void setPlaybackRate(float rate);
  float getPlaybackRate() const { return _playbackRate; }

  void setSpeedFromPulseUs(uint16_t us,
                           uint16_t inMinUs = 1000, uint16_t inMaxUs = 2000,
                           float rateMin = 0.7f, float rateMax = 1.8f);

  // Debug helpers
  uint32_t getWavSampleRate() const { return _wavSampleRate; }
  uint16_t debugBuffered() const { return TMRpcmSpeed32u4::_count; }

  // ISR entry point (must be public)
  static void _isrService();

private:
  bool _openAndParseWav(const char* filename);
  void _skip(uint32_t n);

  void _setupPwmTimer4();
  void _forceTimer1Ctc(uint32_t sampleRate);
  void _applyRateToTimer1();

  // Ring buffer (power of 2 recommended)
  static const uint16_t BUF_SZ = 256;

  static volatile uint8_t  _buf[BUF_SZ];
  static volatile uint16_t _rd;
  static volatile uint16_t _wr;
  static volatile uint16_t _count;

  static TMRpcmSpeed32u4* _active;

  File _file;
  char _currentName[32] = {0};

  // WAV data window
  uint32_t _dataStart = 0;
  uint32_t _dataSize  = 0;
  uint32_t _dataPos   = 0;

  uint32_t _wavSampleRate = 8000;
  uint16_t _baseOcr1a = 0;

  uint8_t  _csPin = 10;
  uint8_t  _volumeShift = 0;

  volatile bool _playing = false;
  volatile bool _paused  = false;

  volatile float _playbackRate = 1.0f;
};
