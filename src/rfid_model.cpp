#include "rfid_model.h"

#include <cstdio>
#include <sstream>
#include <stdexcept>

namespace rfid {

std::string cardFamilyLabel(CardFamily family) {
  switch (family) {
    case CardFamily::ClassicMini:
      return "MIFARE Mini";
    case CardFamily::Classic1K:
      return "MIFARE Classic 1K";
    case CardFamily::Classic4K:
      return "MIFARE Classic 4K";
    case CardFamily::Ultralight:
      return "MIFARE Ultralight";
    case CardFamily::UltralightC:
      return "MIFARE Ultralight C";
    case CardFamily::NTAG:
      return "NTAG";
    case CardFamily::DESFire:
      return "DESFire";
    case CardFamily::Unknown:
    default:
      return "Unknown";
  }
}

bool supportsClassicOps(CardFamily family) {
  return family == CardFamily::ClassicMini || family == CardFamily::Classic1K ||
         family == CardFamily::Classic4K;
}

bool supportsUidRewrite(CardFamily family) {
  return supportsClassicOps(family);
}

std::string bytesToHex(const uint8_t* bytes, std::size_t size, bool spaced) {
  std::ostringstream out;
  for (std::size_t i = 0; i < size; ++i) {
    if (spaced && i > 0) {
      out << ' ';
    }
    char buffer[3];
    std::snprintf(buffer, sizeof(buffer), "%02X", bytes[i]);
    out << buffer;
  }
  return out.str();
}

std::string formatUid(const uint8_t* bytes, std::size_t size, bool spaced) {
  return bytesToHex(bytes, size, spaced);
}

int sectorCount(CardFamily family) {
  switch (family) {
    case CardFamily::ClassicMini:
      return 5;
    case CardFamily::Classic1K:
      return 16;
    case CardFamily::Classic4K:
      return 40;
    default:
      return 0;
  }
}

int blocksInSector(int sector, CardFamily family) {
  if (sector < 0) {
    return 0;
  }
  if (!supportsClassicOps(family)) {
    return 0;
  }
  if (family == CardFamily::Classic4K && sector >= 32) {
    return 16;
  }
  return 4;
}

int firstBlockForSector(int sector, CardFamily family) {
  if (sector < 0 || !supportsClassicOps(family)) {
    return -1;
  }
  if (family == CardFamily::Classic4K && sector >= 32) {
    return 128 + ((sector - 32) * 16);
  }
  return sector * 4;
}

int trailerBlockForSector(int sector, CardFamily family) {
  const int first = firstBlockForSector(sector, family);
  const int blocks = blocksInSector(sector, family);
  if (first < 0 || blocks <= 0) {
    return -1;
  }
  return first + blocks - 1;
}

bool isSectorTrailerBlock(int block, CardFamily family) {
  if (block < 0 || !supportsClassicOps(family)) {
    return false;
  }
  if (family == CardFamily::Classic4K && block >= 128) {
    return ((block - 128 + 1) % 16) == 0;
  }
  return ((block + 1) % 4) == 0;
}

int sectorForBlock(int block, CardFamily family) {
  if (block < 0 || !supportsClassicOps(family)) {
    return -1;
  }
  if (family == CardFamily::Classic4K && block >= 128) {
    return 32 + ((block - 128) / 16);
  }
  return block / 4;
}

}  // namespace rfid
