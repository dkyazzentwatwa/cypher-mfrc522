#include <array>
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include "command_parser.h"
#include "dump_format.h"
#include "rfid_model.h"

int main() {
  using namespace rfid;

  {
    const std::array<uint8_t, 4> uid{0xDE, 0xAD, 0xBE, 0xEF};
    assert(formatUid(uid.data(), uid.size()) == "DE AD BE EF");
    assert(formatUid(uid.data(), uid.size(), false) == "DEADBEEF");
  }

  {
    assert(firstBlockForSector(0, CardFamily::Classic1K) == 0);
    assert(trailerBlockForSector(0, CardFamily::Classic1K) == 3);
    assert(firstBlockForSector(1, CardFamily::Classic1K) == 4);
    assert(trailerBlockForSector(1, CardFamily::Classic1K) == 7);
    assert(blocksInSector(32, CardFamily::Classic4K) == 16);
    assert(trailerBlockForSector(32, CardFamily::Classic4K) == 143);
    assert(trailerBlockForSector(39, CardFamily::Classic4K) == 255);
  }

  {
    Command command;
    assert(parseCommand("dump save backup-01", command));
    assert(command.type == CommandType::DumpSave);
    assert(command.args[0] == "backup-01");
    assert(parseCommand("magic set-uid DE AD BE EF", command));
    assert(command.type == CommandType::MagicSetUid);
  }

  {
    DumpMetadata metadata;
    metadata.schemaVersion = "1";
    metadata.deviceName = "cypher-mfrc522";
    metadata.uid = "DE AD BE EF";
    metadata.cardType = "MIFARE Classic 1K";
    metadata.blockCount = 64;

    std::vector<std::array<uint8_t, 16>> blocks;
    blocks.push_back({0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                      0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10});

    const std::string serialized = serializeDump(metadata, blocks);
    DumpMetadata parsedMetadata;
    std::vector<std::array<uint8_t, 16>> parsedBlocks;
    assert(parseDump(serialized, parsedMetadata, parsedBlocks));
    assert(parsedMetadata.uid == metadata.uid);
    assert(parsedMetadata.cardType == metadata.cardType);
    assert(parsedBlocks.size() == 1);
    assert(parsedBlocks[0][0] == 0x01);
    assert(parsedBlocks[0][15] == 0x10);
  }

  return 0;
}
