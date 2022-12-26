#pragma once

#include <atomic>
#include <cmath>
#include <memory>

#include "BellUtils.h"
#include "CircularBuffer.h"
#include "StreamInfo.h"

typedef std::function<void(std::string)> shutdownEventHandler;

namespace bell {
class CentralAudioBuffer {
 private:
  std::mutex accessMutex;

  std::atomic<bool> isLocked = false;

 public:
  static const size_t PCM_CHUNK_SIZE = 4096;

  // Audio marker for track change detection, and DSP autoconfig
  struct AudioChunk {
    // Unique track hash, used for track change detection
    size_t trackHash;

    // Audio format
    uint32_t sampleRate;
    uint8_t channels;
    uint8_t bitDepth;

    // PCM data size
    size_t pcmSize;

    // PCM data
    uint8_t pcmData[PCM_CHUNK_SIZE];
  };

  CentralAudioBuffer(size_t chunks) {
    audioBuffer = std::make_shared<CircularBuffer>(chunks * sizeof(AudioChunk));
  }

  std::shared_ptr<bell::CircularBuffer> audioBuffer;
  uint32_t currentSampleRate = 44100;

  /**
	 * Returns current sample rate
	 * @return sample rate
	 */
  uint32_t getSampleRate() { return currentSampleRate; }

  /**
	 * Clears input buffer, to be called for track change and such
	 */
  void clearBuffer() {
    size_t exceptSize = currentSampleRate + (sizeof(AudioChunk) - (currentSampleRate % sizeof(AudioChunk)));
    audioBuffer->emptyExcept(exceptSize);
  }

  bool hasAtLeast(size_t chunks) {
    return this->audioBuffer->size() >= chunks * sizeof(AudioChunk);
  }

  /**
	 * Locks access to audio buffer. Call after starting playback
	 */
  void lockAccess() {
    if (!isLocked) {
      clearBuffer();
      this->accessMutex.lock();
      isLocked = true;
    }
  }

  /**
	 * Frees access to the audio buffer. Call during shutdown
	 */
  void unlockAccess() {
    if (isLocked) {
      clearBuffer();
      this->accessMutex.unlock();
      isLocked = false;
    }
  }

  AudioChunk currentChunk = {.pcmSize = 0};
  bool hasChunk = false;

  AudioChunk lastReadChunk = {.pcmSize = 0};

  AudioChunk readChunk() {
    if (audioBuffer->size() < sizeof(AudioChunk)) {
      lastReadChunk.pcmSize = 0;
      return lastReadChunk;
    }

    auto readBytes =
        audioBuffer->read((uint8_t*)&lastReadChunk, sizeof(AudioChunk));
    currentSampleRate = lastReadChunk.sampleRate;
    return lastReadChunk;
  }

  size_t writePCM(const uint8_t* data, size_t dataSize, size_t hash,
                  uint32_t sampleRate = 44100, uint8_t channels = 2,
                  uint8_t bitDepth = 16) {
    if (hasChunk && (currentChunk.trackHash != hash ||
                     currentChunk.pcmSize >= PCM_CHUNK_SIZE)) {

      if ((audioBuffer->size() - audioBuffer->capacity()) <
          sizeof(AudioChunk)) {
        return 0;
      }

      // Track changed or buf full, return current chunk
      hasChunk = false;
      this->audioBuffer->write((uint8_t*)&currentChunk, sizeof(AudioChunk));
    }

		// New chunk requested, initialize
    if (!hasChunk) {
      currentChunk.trackHash = hash;
      currentChunk.sampleRate = sampleRate;
      currentChunk.channels = channels;
      currentChunk.bitDepth = bitDepth;
      currentChunk.pcmSize = 0;
      hasChunk = true;
    }

		// Calculate how much data we can write
    size_t toWriteSize = dataSize;

    if (currentChunk.pcmSize + toWriteSize > PCM_CHUNK_SIZE) {
      toWriteSize = PCM_CHUNK_SIZE - currentChunk.pcmSize;
    }

		// Copy it over :) 
    memcpy(currentChunk.pcmData + currentChunk.pcmSize, data, toWriteSize);
    currentChunk.pcmSize += toWriteSize;

    return toWriteSize;
  }
};

}  // namespace bell
