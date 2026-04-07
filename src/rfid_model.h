#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace rfid {

enum class CardFamily : uint8_t {
  Unknown = 0,
  ClassicMini,
  Classic1K,
  Classic4K,
  Ultralight,
  UltralightC,
  NTAG,
  DESFire,
};

std::string cardFamilyLabel(CardFamily family);
bool supportsClassicOps(CardFamily family);
bool supportsUidRewrite(CardFamily family);

std::string formatUid(const uint8_t* bytes, std::size_t size, bool spaced = true);
std::string bytesToHex(const uint8_t* bytes, std::size_t size, bool spaced = true);

int sectorCount(CardFamily family);
int blocksInSector(int sector, CardFamily family);
int firstBlockForSector(int sector, CardFamily family);
int trailerBlockForSector(int sector, CardFamily family);
bool isSectorTrailerBlock(int block, CardFamily family);
int sectorForBlock(int block, CardFamily family);

}  // namespace rfid
