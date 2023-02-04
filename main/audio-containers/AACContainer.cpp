#include "AACContainer.h"

using namespace bell;

#define SYNC_WORLD_LEN 4

AACContainer::AACContainer(std::istream& istr) : bell::AudioContainer(istr) {}

bool AACContainer::fillBuffer() {
  if (this->bytesInBuffer < AAC_MAX_FRAME_SIZE * 2) {
    this->istr.read((char*)buffer.data() + bytesInBuffer,
                    buffer.size() - bytesInBuffer);
    this->bytesInBuffer += istr.gcount();
  }
  return this->bytesInBuffer >= AAC_MAX_FRAME_SIZE * 2;
}

std::byte* AACContainer::readSample(uint32_t& len) {
  // Align the data if previous read was offseted
  if (dataOffset > 0) {
    memmove(buffer.data(), buffer.data() + dataOffset,
            bytesInBuffer - dataOffset);
    bytesInBuffer = bytesInBuffer - dataOffset;
    dataOffset = 0;
  }

  if (!this->fillBuffer()) {
    len = 0;
    return nullptr;
  }

  int startOffset =
      AACFindSyncWord((uint8_t*)this->buffer.data(), bytesInBuffer);

  if (startOffset < 0) {
    // Discard buffer
    this->bytesInBuffer = 0;
    len = 0;
    return nullptr;
  }

  dataOffset = AACFindSyncWord(
      (uint8_t*)this->buffer.data() + startOffset + SYNC_WORLD_LEN,
      bytesInBuffer - startOffset - SYNC_WORLD_LEN);
  if (dataOffset < 0) {
    // Discard buffer
    this->bytesInBuffer = 0;
    len = 0;
    return nullptr;
  }

  len = dataOffset + SYNC_WORLD_LEN;
  dataOffset += startOffset + SYNC_WORLD_LEN;

  if (len == 0) {
    return nullptr;
  }

  return this->buffer.data() + startOffset;
}

void AACContainer::parseSetupData() {
  channels = 2;
  sampleRate = bell::SampleRate::SR_44100;
  bitWidth = bell::BitWidth::BW_16;
}
