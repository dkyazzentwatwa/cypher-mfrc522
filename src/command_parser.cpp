#include "command_parser.h"

#include <cctype>
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

std::vector<std::string> split(const std::string& value) {
  std::istringstream input(value);
  std::vector<std::string> tokens;
  std::string token;
  while (input >> token) {
    tokens.push_back(token);
  }
  return tokens;
}

bool startsWith(const std::vector<std::string>& tokens, const std::string& token) {
  return !tokens.empty() && tokens[0] == token;
}

}  // namespace

std::string commandTypeLabel(CommandType type) {
  switch (type) {
    case CommandType::Help:
      return "help";
    case CommandType::ReaderInfo:
      return "reader info";
    case CommandType::ReaderSelfTest:
      return "reader selftest";
    case CommandType::Inspect:
      return "inspect";
    case CommandType::ReadBlock:
      return "read-block";
    case CommandType::WriteBlock:
      return "write-block";
    case CommandType::DumpSave:
      return "dump save";
    case CommandType::DumpLoad:
      return "dump load";
    case CommandType::KeyList:
      return "keys list";
    case CommandType::KeySelect:
      return "keys select";
    case CommandType::KeyImport:
      return "keys import";
    case CommandType::MagicSetUid:
      return "magic set-uid";
    case CommandType::Unknown:
    default:
      return "unknown";
  }
}

bool parseCommand(const std::string& line, Command& command) {
  command = {};
  const std::string cleaned = trim(line);
  if (cleaned.empty()) {
    return false;
  }

  const std::vector<std::string> tokens = split(cleaned);
  if (tokens.empty()) {
    return false;
  }

  if (tokens[0] == "help") {
    command.type = CommandType::Help;
    return true;
  }
  if (tokens[0] == "inspect") {
    command.type = CommandType::Inspect;
    return true;
  }
  if (tokens[0] == "reader" && tokens.size() >= 2) {
    if (tokens[1] == "info") {
      command.type = CommandType::ReaderInfo;
      return true;
    }
    if (tokens[1] == "selftest") {
      command.type = CommandType::ReaderSelfTest;
      return true;
    }
  }
  if (tokens[0] == "read-block" && tokens.size() >= 2) {
    command.type = CommandType::ReadBlock;
    command.args.assign(tokens.begin() + 1, tokens.end());
    return true;
  }
  if (tokens[0] == "write-block" && tokens.size() >= 3) {
    command.type = CommandType::WriteBlock;
    command.args.assign(tokens.begin() + 1, tokens.end());
    return true;
  }
  if (tokens[0] == "dump" && tokens.size() >= 3) {
    if (tokens[1] == "save") {
      command.type = CommandType::DumpSave;
      command.args.assign(tokens.begin() + 2, tokens.end());
      return true;
    }
    if (tokens[1] == "load") {
      command.type = CommandType::DumpLoad;
      command.args.assign(tokens.begin() + 2, tokens.end());
      return true;
    }
  }
  if (tokens[0] == "keys" && tokens.size() >= 2) {
    if (tokens[1] == "list") {
      command.type = CommandType::KeyList;
      return true;
    }
    if (tokens[1] == "select" && tokens.size() >= 3) {
      command.type = CommandType::KeySelect;
      command.args.assign(tokens.begin() + 2, tokens.end());
      return true;
    }
    if (tokens[1] == "import" && tokens.size() >= 3) {
      command.type = CommandType::KeyImport;
      command.args.assign(tokens.begin() + 2, tokens.end());
      return true;
    }
  }
  if (tokens[0] == "magic" && tokens.size() >= 2 && tokens[1] == "set-uid") {
    command.type = CommandType::MagicSetUid;
    command.args.assign(tokens.begin() + 2, tokens.end());
    return true;
  }

  return false;
}

}  // namespace rfid
