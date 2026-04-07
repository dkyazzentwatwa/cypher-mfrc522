#include "dump_format.h"

#include <cctype>
#include <iomanip>
#include <sstream>

namespace rfid {
namespace {

std::string trim(const std::string& value) {
  std::size_t start = 0;
  while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }
  std::size_t end = value.size();
  while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }
  return value.substr(start, end - start);
}

bool startsWith(const std::string& value, const std::string& prefix) {
  return value.compare(0, prefix.size(), prefix) == 0;
}

std::string hexByte(std::uint8_t value) {
  std::ostringstream out;
  out << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
      << static_cast<int>(value);
  return out.str();
}

bool parseHexByte(const std::string& token, std::uint8_t& value) {
  std::string cleaned;
  for (char ch : token) {
    if (std::isxdigit(static_cast<unsigned char>(ch))) {
      cleaned.push_back(ch);
    }
  }
  if (cleaned.empty() || cleaned.size() > 2) {
    return false;
  }
  std::istringstream input(cleaned);
  unsigned int parsed = 0;
  input >> std::hex >> parsed;
  if (!input || parsed > 0xFF) {
    return false;
  }
  value = static_cast<std::uint8_t>(parsed);
  return true;
}

}  // namespace

std::string serializeDump(const DumpMetadata& metadata, const std::vector<DumpBlock>& blocks) {
  std::ostringstream out;
  out << "CYFER-MFRC522-DUMP\n";
  out << "schema=" << metadata.schemaVersion << "\n";
  out << "device=" << metadata.deviceName << "\n";
  out << "uid=" << metadata.uid << "\n";
  out << "cardType=" << metadata.cardType << "\n";
  out << "unreadableBlocks=" << metadata.unreadableBlocks << "\n";
  out << "blocks=" << metadata.blockCount << "\n";
  out << "\n";
  for (std::size_t index = 0; index < blocks.size(); ++index) {
    out << "block " << index << ":";
    for (std::size_t byteIndex = 0; byteIndex < blocks[index].size(); ++byteIndex) {
      out << ' ' << hexByte(blocks[index][byteIndex]);
    }
    out << "\n";
  }
  return out.str();
}

bool parseDump(const std::string& text, DumpMetadata& metadata, std::vector<DumpBlock>& blocks) {
  metadata = {};
  blocks.clear();

  std::istringstream input(text);
  std::string line;
  bool sawHeader = false;

  while (std::getline(input, line)) {
    line = trim(line);
    if (line.empty()) {
      continue;
    }
    if (!sawHeader) {
      if (line != "CYFER-MFRC522-DUMP") {
        return false;
      }
      sawHeader = true;
      continue;
    }
    if (startsWith(line, "schema=")) {
      metadata.schemaVersion = line.substr(7);
      continue;
    }
    if (startsWith(line, "device=")) {
      metadata.deviceName = line.substr(7);
      continue;
    }
    if (startsWith(line, "uid=")) {
      metadata.uid = line.substr(4);
      continue;
    }
    if (startsWith(line, "cardType=")) {
      metadata.cardType = line.substr(9);
      continue;
    }
    if (startsWith(line, "unreadableBlocks=")) {
      metadata.unreadableBlocks = line.substr(17);
      continue;
    }
    if (startsWith(line, "blocks=")) {
      metadata.blockCount = static_cast<std::uint32_t>(std::stoul(line.substr(7)));
      continue;
    }
    if (startsWith(line, "block ")) {
      const std::size_t colon = line.find(':');
      if (colon == std::string::npos) {
        return false;
      }
      std::istringstream values(line.substr(colon + 1));
      DumpBlock block{};
      for (std::size_t i = 0; i < block.size(); ++i) {
        std::string token;
        if (!(values >> token)) {
          return false;
        }
        std::uint8_t byte = 0;
        if (!parseHexByte(token, byte)) {
          return false;
        }
        block[i] = byte;
      }
      blocks.push_back(block);
      continue;
    }
  }

  return sawHeader;
}

}  // namespace rfid
