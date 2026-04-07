#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace rfid {

struct DumpMetadata {
  std::string schemaVersion;
  std::string deviceName;
  std::string uid;
  std::string cardType;
  std::string unreadableBlocks;
  std::uint32_t blockCount = 0;
};

using DumpBlock = std::array<std::uint8_t, 16>;

std::string serializeDump(const DumpMetadata& metadata, const std::vector<DumpBlock>& blocks);
bool parseDump(const std::string& text, DumpMetadata& metadata, std::vector<DumpBlock>& blocks);

}  // namespace rfid
