#pragma once

#include <string>
#include <vector>

namespace rfid {

enum class CommandType {
  Unknown = 0,
  Help,
  ReaderInfo,
  ReaderSelfTest,
  Inspect,
  ReadBlock,
  WriteBlock,
  DumpSave,
  DumpLoad,
  KeyList,
  KeySelect,
  KeyImport,
  MagicSetUid,
};

struct Command {
  CommandType type = CommandType::Unknown;
  std::vector<std::string> args;
};

bool parseCommand(const std::string& line, Command& command);
std::string commandTypeLabel(CommandType type);

}  // namespace rfid
