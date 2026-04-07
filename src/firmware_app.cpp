#include "firmware_app.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace {

constexpr byte kKnownKeyValues[][MFRC522::MF_KEY_SIZE] = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5},
    {0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5},
    {0x4D, 0x3A, 0x99, 0xC3, 0x51, 0xDD},
    {0x1A, 0x98, 0x2C, 0x7E, 0x45, 0x9A},
    {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7},
    {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
};

bool containsBlockIndex(const String& csv, int block) {
  if (csv.length() == 0) {
    return false;
  }
  const String needle = String(block);
  int start = 0;
  while (start < static_cast<int>(csv.length())) {
    int comma = csv.indexOf(',', start);
    if (comma < 0) {
      comma = csv.length();
    }
    const String token = csv.substring(start, comma);
    if (token == needle) {
      return true;
    }
    start = comma + 1;
  }
  return false;
}

String bytesToAscii(const byte* data, std::size_t len) {
  String out;
  for (std::size_t i = 0; i < len; ++i) {
    const byte c = data[i];
    out += (c >= 0x20 && c <= 0x7E) ? static_cast<char>(c) : '.';
  }
  return out;
}

}  // namespace

void FirmwareApp::begin() {
  setupButtons();
  setupKeys();

  Serial.begin(115200);
  delay(100);

  Wire.begin(cypher::PIN_OLED_SDA, cypher::PIN_OLED_SCL);
  Wire.setClock(400000);

  setupDisplay();
  setupReader();
  setupStorage();

  Serial.println();
  Serial.println(F("CYPHER MFRC522 initialized"));
  Serial.println(F("Type 'help' for serial commands."));

  showMessage("Ready", "Inspect, read,", "dump, restore,", "diagnostics");
}

void FirmwareApp::tick() {
  pollButtons();
  pollSerial();

  if (screen_ == Screen::Message && messageUntil_ != 0 && millis() > messageUntil_) {
    messageUntil_ = 0;
    screen_ = Screen::Home;
  }

  if (screen_ == Screen::Inspect) {
    refreshCardInfo();
  }

  render();
}

void FirmwareApp::setupButtons() {
  buttonUp_.pin = cypher::PIN_BUTTON_UP;
  buttonDown_.pin = cypher::PIN_BUTTON_DOWN;
  buttonSelect_.pin = cypher::PIN_BUTTON_SELECT;

  pinMode(buttonUp_.pin, INPUT_PULLUP);
  pinMode(buttonDown_.pin, INPUT_PULLUP);
  pinMode(buttonSelect_.pin, INPUT_PULLUP);
}

void FirmwareApp::setupKeys() {
  for (std::size_t keyIndex = 0; keyIndex < kKnownKeyCount; ++keyIndex) {
    for (std::size_t i = 0; i < MFRC522::MF_KEY_SIZE; ++i) {
      knownKeys_[keyIndex].keyByte[i] = kKnownKeyValues[keyIndex][i];
      activeKey_.keyByte[i] = 0xFF;
    }
  }
}

void FirmwareApp::setupDisplay() {
  if (!display_.begin(SSD1306_SWITCHCAPVCC, cypher::OLED_I2C_ADDRESS)) {
    Serial.println(F("OLED allocation failed"));
    displayReady_ = false;
    return;
  }
  displayReady_ = true;
  display_.clearDisplay();
  display_.display();
}

void FirmwareApp::setupReader() {
  SPI.begin(cypher::PIN_SPI_SCK, cypher::PIN_SPI_MISO, cypher::PIN_SPI_MOSI, cypher::PIN_RFID_SS);
  reader_.PCD_Init();
  reader_.PCD_SetAntennaGain(reader_.RxGain_max);

  const byte version = reader_.PCD_ReadRegister(reader_.VersionReg);
  readerReady_ = version != 0x00 && version != 0xFF;

  if (!readerReady_) {
    Serial.println(F("MFRC522 not detected or miswired"));
    showMessage("Reader error", "Check wiring", "MFRC522 not", "responding", 0);
  }
}

void FirmwareApp::setupStorage() {
  sdReady_ = SD.begin(cypher::PIN_SD_CS);
  if (!sdReady_) {
    Serial.println(F("SD init failed; dump save/restore disabled"));
  }
}

void FirmwareApp::render() {
  if (!displayReady_) {
    return;
  }

  display_.clearDisplay();
  display_.drawRect(0, 0, cypher::OLED_WIDTH, cypher::OLED_HEIGHT, SSD1306_WHITE);
  display_.setTextColor(SSD1306_WHITE);
  display_.setTextSize(1);
  display_.setCursor(4, 4);

  switch (screen_) {
    case Screen::Home:
      renderHome();
      break;
    case Screen::Inspect:
      renderInspect();
      break;
    case Screen::ReadBlockSelect:
      renderReadBlockSelect();
      break;
    case Screen::ReadBlockResult:
      renderReadBlockResult();
      break;
    case Screen::DumpSave:
      renderDumpSave();
      break;
    case Screen::RestorePick:
      renderRestorePick();
      break;
    case Screen::Diagnostics:
      renderDiagnostics();
      break;
    case Screen::About:
      renderAbout();
      break;
    case Screen::Message:
      renderMessage();
      break;
  }

  display_.display();
}

void FirmwareApp::renderHome() {
  display_.println(F("MFRC522 Menu"));
  display_.drawLine(0, 14, cypher::OLED_WIDTH, 14, SSD1306_WHITE);

  const int visibleCount = 4;
  const int itemCount = sizeof(menuItems_) / sizeof(menuItems_[0]);
  if (selectedMenu_ < menuScroll_) {
    menuScroll_ = selectedMenu_;
  } else if (selectedMenu_ >= menuScroll_ + visibleCount) {
    menuScroll_ = selectedMenu_ - visibleCount + 1;
  }

  for (int i = 0; i < visibleCount; ++i) {
    const int index = menuScroll_ + i;
    if (index >= itemCount) {
      break;
    }
    display_.setCursor(10, 18 + (i * 12));
    display_.print(index == selectedMenu_ ? F("> ") : F("  "));
    display_.println(menuItems_[index].label);
  }

  if (menuScroll_ > 0) {
    display_.fillTriangle(120, 16, 124, 16, 122, 13, SSD1306_WHITE);
  }
  if (menuScroll_ + visibleCount < itemCount) {
    display_.fillTriangle(120, 60, 124, 60, 122, 63, SSD1306_WHITE);
  }
}

void FirmwareApp::renderInspect() {
  display_.println(F("Inspect card"));
  display_.drawLine(0, 14, cypher::OLED_WIDTH, 14, SSD1306_WHITE);
  if (!currentCardValid_) {
    display_.setCursor(4, 20);
    display_.println(F("Present a card"));
    display_.setCursor(4, 34);
    display_.println(F("SELECT to exit"));
    return;
  }

  display_.setCursor(4, 18);
  display_.println(currentCard_.uidText);
  display_.setCursor(4, 30);
  display_.println(currentCard_.typeText);
  display_.setCursor(4, 42);
  display_.println(currentCard_.classic ? F("Classic ops: yes") : F("Classic ops: no"));
  display_.setCursor(4, 54);
  display_.println(F("SELECT to exit"));
}

void FirmwareApp::renderReadBlockSelect() {
  display_.println(F("Read block"));
  display_.drawLine(0, 14, cypher::OLED_WIDTH, 14, SSD1306_WHITE);
  display_.setCursor(4, 20);
  display_.print(F("Block: "));
  display_.println(selectedBlock_);
  display_.setCursor(4, 32);
  display_.println(F("UP/DOWN changes"));
  display_.setCursor(4, 44);
  display_.println(F("SELECT reads"));
  display_.setCursor(4, 56);
  display_.println(F("SELECT hold exits"));
}

void FirmwareApp::renderReadBlockResult() {
  display_.println(F("Block result"));
  display_.drawLine(0, 14, cypher::OLED_WIDTH, 14, SSD1306_WHITE);
  if (!currentCardValid_) {
    display_.setCursor(4, 20);
    display_.println(F("No card"));
    return;
  }
  display_.setCursor(4, 18);
  display_.print(F("B"));
  display_.print(selectedBlock_);
  display_.print(F(": "));
  display_.setCursor(4, 30);
  display_.println(messageLine1_);
  display_.setCursor(4, 42);
  display_.println(messageLine2_);
  display_.setCursor(4, 54);
  display_.println(F("SELECT to exit"));
}

void FirmwareApp::renderDumpSave() {
  display_.println(F("Save dump"));
  display_.drawLine(0, 14, cypher::OLED_WIDTH, 14, SSD1306_WHITE);
  display_.setCursor(4, 20);
  display_.println(sdReady_ ? F("SELECT saves now") : F("SD unavailable"));
  display_.setCursor(4, 32);
  display_.println(lastDumpFile_);
  display_.setCursor(4, 44);
  display_.println(F("Serial: dump save"));
  display_.setCursor(4, 56);
  display_.println(F("SELECT to exit"));
}

void FirmwareApp::renderRestorePick() {
  display_.println(F("Restore dump"));
  display_.drawLine(0, 14, cypher::OLED_WIDTH, 14, SSD1306_WHITE);
  if (dumpFiles_.empty()) {
    display_.setCursor(4, 20);
    display_.println(F("No .dump files"));
    display_.setCursor(4, 32);
    display_.println(F("SELECT to refresh"));
    return;
  }

  if (selectedDumpIndex_ < 0) {
    selectedDumpIndex_ = 0;
  }
  if (selectedDumpIndex_ >= static_cast<int>(dumpFiles_.size())) {
    selectedDumpIndex_ = dumpFiles_.size() - 1;
  }

  const int visibleCount = 3;
  int scroll = selectedDumpIndex_ - 1;
  if (scroll < 0) {
    scroll = 0;
  }
  if (scroll + visibleCount > static_cast<int>(dumpFiles_.size())) {
    scroll = std::max(0, static_cast<int>(dumpFiles_.size()) - visibleCount);
  }

  for (int i = 0; i < visibleCount; ++i) {
    const int index = scroll + i;
    if (index >= static_cast<int>(dumpFiles_.size())) {
      break;
    }
    display_.setCursor(4, 18 + (i * 12));
    display_.print(index == selectedDumpIndex_ ? F("> ") : F("  "));
    display_.println(dumpFiles_[index]);
  }
  display_.setCursor(4, 56);
  display_.println(F("SELECT restores"));
}

void FirmwareApp::renderDiagnostics() {
  display_.println(F("Diagnostics"));
  display_.drawLine(0, 14, cypher::OLED_WIDTH, 14, SSD1306_WHITE);
  display_.setCursor(4, 18);
  display_.print(F("Reader: "));
  display_.println(readerReady_ ? F("ok") : F("missing"));
  display_.setCursor(4, 30);
  display_.print(F("SD: "));
  display_.println(sdReady_ ? F("ok") : F("missing"));
  display_.setCursor(4, 42);
  display_.print(F("Gain: "));
  display_.println(reader_.PCD_GetAntennaGain());
  display_.setCursor(4, 54);
  display_.println(F("SELECT to exit"));
}

void FirmwareApp::renderAbout() {
  display_.println(F("About"));
  display_.drawLine(0, 14, cypher::OLED_WIDTH, 14, SSD1306_WHITE);
  display_.setCursor(4, 18);
  display_.println(F("MFRC522 tool"));
  display_.setCursor(4, 30);
  display_.println(F("Classic-first"));
  display_.setCursor(4, 42);
  display_.println(F("Hybrid UI + serial"));
  display_.setCursor(4, 54);
  display_.println(F("SELECT to exit"));
}

void FirmwareApp::renderMessage() {
  display_.println(messageTitle_);
  display_.drawLine(0, 14, cypher::OLED_WIDTH, 14, SSD1306_WHITE);
  display_.setCursor(4, 18);
  display_.println(messageLine1_);
  display_.setCursor(4, 30);
  display_.println(messageLine2_);
  display_.setCursor(4, 42);
  display_.println(messageLine3_);
  display_.setCursor(4, 54);
  display_.println(F("SELECT exits"));
}

void FirmwareApp::pollButtons() {
  if (updateButton(buttonUp_) && buttonUp_.pressedEdge) {
    if (screen_ == Screen::Home) {
      selectedMenu_ = (selectedMenu_ - 1 + (sizeof(menuItems_) / sizeof(menuItems_[0]))) %
                      (sizeof(menuItems_) / sizeof(menuItems_[0]));
    } else if (screen_ == Screen::ReadBlockSelect) {
      selectedBlock_ = (selectedBlock_ + 1) % 64;
    } else if (screen_ == Screen::RestorePick && !dumpFiles_.empty()) {
      selectedDumpIndex_ = (selectedDumpIndex_ + 1) % dumpFiles_.size();
    }
  }

  if (updateButton(buttonDown_) && buttonDown_.pressedEdge) {
    if (screen_ == Screen::Home) {
      selectedMenu_ = (selectedMenu_ + 1) % (sizeof(menuItems_) / sizeof(menuItems_[0]));
    } else if (screen_ == Screen::ReadBlockSelect) {
      selectedBlock_ = (selectedBlock_ - 1 + 64) % 64;
    } else if (screen_ == Screen::RestorePick && !dumpFiles_.empty()) {
      selectedDumpIndex_ = (selectedDumpIndex_ - 1 + dumpFiles_.size()) % dumpFiles_.size();
    }
  }

  if (updateButton(buttonSelect_) && buttonSelect_.pressedEdge) {
    if (screen_ == Screen::Home) {
      screen_ = menuItems_[selectedMenu_].target;
      if (screen_ == Screen::RestorePick) {
        dumpFiles_ = listDumpFiles();
      }
      if (screen_ == Screen::Inspect) {
        refreshCardInfo();
      }
      return;
    }

    if (screen_ == Screen::Inspect || screen_ == Screen::About || screen_ == Screen::Diagnostics) {
      screen_ = Screen::Home;
      return;
    }

    if (screen_ == Screen::DumpSave) {
      if (!refreshCardInfo() || !currentCard_.classic) {
        showMessage("Save dump", "Present a Classic", "card first", "", 1800);
        return;
      }
      saveDumpToSd(currentCard_);
      screen_ = Screen::Home;
      return;
    }

    if (screen_ == Screen::ReadBlockSelect) {
    if (!refreshCardInfo()) {
      showMessage("Read block", "No card present", "", "SELECT exits", 1200);
      return;
      }

      if (!currentCard_.classic) {
        showMessage("Unsupported", "Classic cards only", currentCard_.typeText, "", 1800);
        return;
      }

      byte data[16] = {};
      if (readClassicBlock(static_cast<byte>(selectedBlock_), data)) {
        messageTitle_ = "Block " + String(selectedBlock_);
        messageLine1_ = formatBytesAsHex(data, 16);
        messageLine2_ = bytesToAscii(data, 16);
        messageLine3_ = currentCard_.uidText;
        screen_ = Screen::ReadBlockResult;
      } else {
        showMessage("Read failed", "Block " + String(selectedBlock_), "Auth or CRC error", "", 1800);
      }
      return;
    }

    if (screen_ == Screen::ReadBlockResult) {
      screen_ = Screen::ReadBlockSelect;
      return;
    }

    if (screen_ == Screen::RestorePick) {
      if (dumpFiles_.empty()) {
        dumpFiles_ = listDumpFiles();
        return;
      }
      selectedDumpFile_ = dumpFiles_[selectedDumpIndex_];
      if (!restoreDumpFromSd(selectedDumpFile_)) {
        showMessage("Restore failed", selectedDumpFile_, "", "", 2000);
      }
      return;
    }
  }
}

bool FirmwareApp::updateButton(ButtonState& button) {
  const bool rawPressed = digitalRead(button.pin) == LOW;
  button.pressedEdge = false;
  if (rawPressed != button.lastRaw) {
    button.lastRaw = rawPressed;
    button.lastChange = millis();
  }
  if (millis() - button.lastChange >= cypher::BUTTON_DEBOUNCE_MS) {
    if (button.stable != rawPressed) {
      button.stable = rawPressed;
      button.pressedEdge = button.stable;
    }
  }
  return button.stable;
}

bool FirmwareApp::buttonPressed(ButtonState& button) {
  return button.stable;
}

bool FirmwareApp::buttonPressedEdge(ButtonState& button) {
  return button.pressedEdge;
}

void FirmwareApp::pollSerial() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      serialLineReady_ = true;
      const String line = serialBuffer_;
      serialBuffer_ = "";
      if (line.length() > 0) {
        processSerialLine(line);
      }
      continue;
    }
    serialBuffer_ += c;
  }
}

void FirmwareApp::processSerialLine(const String& line) {
  const std::string input = line.c_str();
  rfid::Command command;
  if (!rfid::parseCommand(input, command)) {
    Serial.print(F("ERR unknown command: "));
    Serial.println(line);
    return;
  }

  if (command.type == rfid::CommandType::Help) {
    Serial.println(F("OK commands:"));
    Serial.println(F("  help"));
    Serial.println(F("  reader info"));
    Serial.println(F("  reader selftest"));
    Serial.println(F("  inspect"));
    Serial.println(F("  read-block <n>"));
    Serial.println(F("  write-block <n> <16 hex bytes>"));
    Serial.println(F("  dump save [name]"));
    Serial.println(F("  dump load <name>"));
    Serial.println(F("  keys list"));
    Serial.println(F("  keys select <index>"));
    Serial.println(F("  magic set-uid <4|7|10 hex bytes>"));
    return;
  }

  if (command.type == rfid::CommandType::ReaderInfo) {
    Serial.print(F("OK reader ready="));
    Serial.print(readerReady_ ? F("true") : F("false"));
    Serial.print(F(" sd="));
    Serial.print(sdReady_ ? F("true") : F("false"));
    Serial.print(F(" gain="));
    Serial.println(reader_.PCD_GetAntennaGain());
    return;
  }

  if (command.type == rfid::CommandType::ReaderSelfTest) {
    Serial.print(F("OK selftest="));
    Serial.println(reader_.PCD_PerformSelfTest() ? F("pass") : F("fail"));
    return;
  }

  if (command.type == rfid::CommandType::Inspect) {
    screen_ = Screen::Inspect;
    refreshCardInfo();
    Serial.println(F("OK inspect mode"));
    return;
  }

  if (command.type == rfid::CommandType::ReadBlock) {
    if (command.args.empty()) {
      Serial.println(F("ERR read-block requires a block number"));
      return;
    }
    selectedBlock_ = atoi(command.args[0].c_str());
    if (!refreshCardInfo()) {
      Serial.println(F("ERR no card"));
      return;
    }
    if (!currentCard_.classic) {
      Serial.println(F("ERR unsupported card family"));
      return;
    }
    byte data[16] = {};
    if (readClassicBlock(static_cast<byte>(selectedBlock_), data)) {
      Serial.print(F("OK block "));
      Serial.print(selectedBlock_);
      Serial.print(F(" "));
      Serial.println(formatBytesAsHex(data, 16));
    } else {
      Serial.println(F("ERR read failed"));
    }
    return;
  }

  if (command.type == rfid::CommandType::WriteBlock) {
    if (command.args.size() < 17) {
      Serial.println(F("ERR write-block requires block number + 16 bytes"));
      return;
    }
    selectedBlock_ = atoi(command.args[0].c_str());
    if (!refreshCardInfo() || !currentCard_.classic) {
      Serial.println(F("ERR unsupported card or no card"));
      return;
    }
    std::vector<byte> payload;
    if (!parseHexByteList(std::vector<std::string>(command.args.begin() + 1, command.args.end()), payload) ||
        payload.size() != 16) {
      Serial.println(F("ERR invalid byte payload"));
      return;
    }
    if (writeClassicBlock(static_cast<byte>(selectedBlock_), payload.data())) {
      Serial.println(F("OK write verified"));
    } else {
      Serial.println(F("ERR write failed"));
    }
    return;
  }

  if (command.type == rfid::CommandType::DumpSave) {
    if (!saveDumpToSd(currentCardValid_ ? currentCard_ : CardInfo{})) {
      Serial.println(F("ERR dump save failed"));
    }
    return;
  }

  if (command.type == rfid::CommandType::DumpLoad) {
    if (command.args.empty()) {
      Serial.println(F("ERR dump load requires a file name"));
      return;
    }
    if (!restoreDumpFromSd(command.args[0].c_str())) {
      Serial.println(F("ERR dump load failed"));
    }
    return;
  }

  if (command.type == rfid::CommandType::KeyList) {
    for (std::size_t i = 0; i < kKnownKeyCount; ++i) {
      Serial.print(F("KEY "));
      Serial.print(i);
      Serial.print(F(" "));
      printKey(knownKeys_[i]);
      Serial.println();
    }
    return;
  }

  if (command.type == rfid::CommandType::KeySelect) {
    if (command.args.empty()) {
      Serial.println(F("ERR key select requires an index"));
      return;
    }
    const int index = atoi(command.args[0].c_str());
    if (index < 0 || index >= static_cast<int>(kKnownKeyCount)) {
      Serial.println(F("ERR key index out of range"));
      return;
    }
    activeKey_ = knownKeys_[index];
    Serial.print(F("OK active key="));
    Serial.println(index);
    return;
  }

  if (command.type == rfid::CommandType::KeyImport) {
    Serial.println(F("ERR key import not wired yet"));
    return;
  }

  if (command.type == rfid::CommandType::MagicSetUid) {
    if (!refreshCardInfo() || !currentCard_.classic || !cardCanUseUidBackdoor(currentCard_.piccType)) {
      Serial.println(F("ERR card is not magic-capable"));
      return;
    }
    std::vector<byte> uidBytes;
    if (!parseHexByteList(std::vector<std::string>(command.args.begin(), command.args.end()), uidBytes) ||
        (uidBytes.size() != 4 && uidBytes.size() != 7 && uidBytes.size() != 10)) {
      Serial.println(F("ERR UID must be 4, 7, or 10 bytes"));
      return;
    }
    byte uidBuffer[10] = {};
    for (std::size_t i = 0; i < uidBytes.size(); ++i) {
      uidBuffer[i] = uidBytes[i];
    }
    if (reader_.MIFARE_SetUid(uidBuffer, static_cast<byte>(uidBytes.size()), true)) {
      Serial.println(F("OK UID written"));
    } else {
      Serial.println(F("ERR UID write failed"));
    }
    return;
  }
}

bool FirmwareApp::detectCard(CardInfo& info) {
  if (!reader_.PICC_IsNewCardPresent()) {
    return false;
  }
  if (!reader_.PICC_ReadCardSerial()) {
    return false;
  }

  info.uid = reader_.uid;
  info.piccType = reader_.PICC_GetType(info.uid.sak);
  info.family = classifyFamily(info.piccType);
  info.uidText = String(rfid::formatUid(info.uid.uidByte, info.uid.size).c_str());
  info.typeText = piccTypeToText(info.piccType);
  info.classic = cardHasClassicOps(info.piccType);
  info.magicCapable = cardCanUseUidBackdoor(info.piccType);
  return true;
}

bool FirmwareApp::refreshCardInfo() {
  CardInfo info;
  if (detectCard(info)) {
    currentCard_ = info;
    currentCardValid_ = true;
    return true;
  }
  currentCardValid_ = false;
  return false;
}

rfid::CardFamily FirmwareApp::classifyFamily(MFRC522::PICC_Type type) {
  switch (type) {
    case MFRC522::PICC_TYPE_MIFARE_MINI:
      return rfid::CardFamily::ClassicMini;
    case MFRC522::PICC_TYPE_MIFARE_1K:
      return rfid::CardFamily::Classic1K;
    case MFRC522::PICC_TYPE_MIFARE_4K:
      return rfid::CardFamily::Classic4K;
    case MFRC522::PICC_TYPE_MIFARE_UL:
      return rfid::CardFamily::Ultralight;
    case MFRC522::PICC_TYPE_MIFARE_PLUS:
      return rfid::CardFamily::Unknown;
    case MFRC522::PICC_TYPE_MIFARE_DESFIRE:
      return rfid::CardFamily::DESFire;
    case MFRC522::PICC_TYPE_ISO_14443_4:
      return rfid::CardFamily::Unknown;
    case MFRC522::PICC_TYPE_ISO_18092:
      return rfid::CardFamily::Unknown;
    default:
      return rfid::CardFamily::Unknown;
  }
}

bool FirmwareApp::cardHasClassicOps(MFRC522::PICC_Type type) {
  return type == MFRC522::PICC_TYPE_MIFARE_MINI || type == MFRC522::PICC_TYPE_MIFARE_1K ||
         type == MFRC522::PICC_TYPE_MIFARE_4K;
}

bool FirmwareApp::cardCanUseUidBackdoor(MFRC522::PICC_Type type) {
  return cardHasClassicOps(type);
}

String FirmwareApp::piccTypeToText(MFRC522::PICC_Type type) {
  const __FlashStringHelper* label = MFRC522::PICC_GetTypeName(type);
  return String(label);
}

bool FirmwareApp::tryKnownKeyForSector(byte sector, MFRC522::MIFARE_Key& outKey, byte& outAuthCommand) {
  const int trailer = rfid::trailerBlockForSector(sector, currentCard_.family);
  if (trailer < 0) {
    return false;
  }

  for (byte phase = 0; phase < 2; ++phase) {
    const byte command = phase == 0 ? MFRC522::PICC_CMD_MF_AUTH_KEY_A : MFRC522::PICC_CMD_MF_AUTH_KEY_B;
    for (std::size_t keyIndex = 0; keyIndex < kKnownKeyCount; ++keyIndex) {
      outKey = knownKeys_[keyIndex];
      if (authenticateBlock(static_cast<byte>(trailer), command, outKey)) {
        outAuthCommand = command;
        return true;
      }
    }
  }
  return false;
}

bool FirmwareApp::authenticateBlock(byte block, byte authCommand, const MFRC522::MIFARE_Key& key) {
  MFRC522::MIFARE_Key temp = key;
  const MFRC522::StatusCode status = reader_.PCD_Authenticate(authCommand, block, &temp, &(reader_.uid));
  return status == MFRC522::STATUS_OK;
}

bool FirmwareApp::readClassicBlock(byte block, byte* out) {
  if (!currentCardValid_ || !currentCard_.classic) {
    return false;
  }
  const int sector = rfid::sectorForBlock(block, currentCard_.family);
  if (sector < 0) {
    return false;
  }

  MFRC522::MIFARE_Key key{};
  byte authCommand = MFRC522::PICC_CMD_MF_AUTH_KEY_A;
  if (!tryKnownKeyForSector(static_cast<byte>(sector), key, authCommand)) {
    return false;
  }

  byte buffer[18];
  byte size = sizeof(buffer);
  if (reader_.MIFARE_Read(block, buffer, &size) != MFRC522::STATUS_OK) {
    reader_.PCD_StopCrypto1();
    return false;
  }

  for (byte i = 0; i < 16; ++i) {
    out[i] = buffer[i];
  }
  reader_.PCD_StopCrypto1();
  reader_.PICC_HaltA();
  return true;
}

bool FirmwareApp::writeClassicBlock(byte block, const byte* data) {
  if (!currentCardValid_ || !currentCard_.classic) {
    return false;
  }
  const int sector = rfid::sectorForBlock(block, currentCard_.family);
  if (sector < 0) {
    return false;
  }

  MFRC522::MIFARE_Key key{};
  byte authCommand = MFRC522::PICC_CMD_MF_AUTH_KEY_A;
  if (!tryKnownKeyForSector(static_cast<byte>(sector), key, authCommand)) {
    return false;
  }

  if (reader_.MIFARE_Write(block, const_cast<byte*>(data), 16) != MFRC522::STATUS_OK) {
    reader_.PCD_StopCrypto1();
    return false;
  }

  byte verify[18];
  byte verifySize = sizeof(verify);
  const bool readOk = reader_.MIFARE_Read(block, verify, &verifySize) == MFRC522::STATUS_OK;
  reader_.PCD_StopCrypto1();
  reader_.PICC_HaltA();
  if (!readOk) {
    return false;
  }
  return std::equal(verify, verify + 16, data);
}

bool FirmwareApp::readClassicSector(byte sector, std::vector<rfid::DumpBlock>& blocks, String& unreadableMask) {
  blocks.clear();
  unreadableMask = "";

  if (!currentCardValid_ || !currentCard_.classic) {
    return false;
  }

  const int firstBlock = rfid::firstBlockForSector(sector, currentCard_.family);
  const int blockCount = rfid::blocksInSector(sector, currentCard_.family);
  if (firstBlock < 0 || blockCount <= 0) {
    return false;
  }

  MFRC522::MIFARE_Key key{};
  byte authCommand = MFRC522::PICC_CMD_MF_AUTH_KEY_A;
  if (!tryKnownKeyForSector(static_cast<byte>(sector), key, authCommand)) {
    for (int i = 0; i < blockCount; ++i) {
      if (unreadableMask.length() > 0) {
        unreadableMask += ",";
      }
      unreadableMask += String(firstBlock + i);
    }
    for (int i = 0; i < blockCount; ++i) {
      blocks.push_back({});
    }
    return true;
  }

  for (int i = 0; i < blockCount; ++i) {
    const byte block = static_cast<byte>(firstBlock + i);
    byte buffer[18];
    byte size = sizeof(buffer);
    rfid::DumpBlock dumpBlock{};
    if (reader_.MIFARE_Read(block, buffer, &size) == MFRC522::STATUS_OK) {
      for (byte b = 0; b < 16; ++b) {
        dumpBlock[b] = buffer[b];
      }
    } else {
      if (unreadableMask.length() > 0) {
        unreadableMask += ",";
      }
      unreadableMask += String(block);
    }
    blocks.push_back(dumpBlock);
  }

  reader_.PCD_StopCrypto1();
  reader_.PICC_HaltA();
  return true;
}

bool FirmwareApp::saveDumpToSd(const CardInfo& info) {
  if (!sdReady_) {
    showMessage("SD unavailable", "Cannot save dump", "", "", 2000);
    return false;
  }
  if (!currentCardValid_ || !currentCard_.classic) {
    showMessage("Save dump", "Present a Classic", "card first", "", 1800);
    return false;
  }

  std::vector<rfid::DumpBlock> blocks;
  String unreadable;
  const int sectors = rfid::sectorCount(currentCard_.family);
  for (int sector = 0; sector < sectors; ++sector) {
    std::vector<rfid::DumpBlock> sectorBlocks;
    String sectorUnreadable;
    if (!readClassicSector(static_cast<byte>(sector), sectorBlocks, sectorUnreadable)) {
      continue;
    }
    blocks.insert(blocks.end(), sectorBlocks.begin(), sectorBlocks.end());
    if (sectorUnreadable.length() > 0) {
      if (unreadable.length() > 0) {
        unreadable += ",";
      }
      unreadable += sectorUnreadable;
    }
  }

  rfid::DumpMetadata metadata;
  metadata.schemaVersion = "1";
  metadata.deviceName = "cypher-mfrc522";
  metadata.uid = currentCard_.uidText.c_str();
  metadata.cardType = currentCard_.typeText.c_str();
  metadata.unreadableBlocks = unreadable.c_str();
  metadata.blockCount = rfid::supportsClassicOps(currentCard_.family) && currentCard_.family == rfid::CardFamily::Classic4K
                            ? 256
                            : (currentCard_.family == rfid::CardFamily::ClassicMini ? 20 : 64);

  const String fileName = nextAutoDumpName(info);
  if (!writeDumpFile(fileName, metadata, blocks)) {
    return false;
  }
  lastDumpFile_ = fileName;
  showMessage("Dump saved", fileName, "Blocks: " + String(blocks.size()), "", 2200);
  return true;
}

bool FirmwareApp::restoreDumpFromSd(const String& fileName) {
  if (!sdReady_) {
    showMessage("SD unavailable", "Cannot restore", "", "", 2000);
    return false;
  }
  if (!currentCardValid_ || !currentCard_.classic) {
    showMessage("Restore dump", "Present a Classic", "target card", "", 1800);
    return false;
  }

  rfid::DumpMetadata metadata;
  std::vector<rfid::DumpBlock> blocks;
  if (!readDumpFile(fileName, metadata, blocks)) {
    showMessage("Restore dump", "Could not read", fileName, "", 2000);
    return false;
  }

  std::vector<int> unreadable;
  if (!metadata.unreadableBlocks.empty()) {
    std::size_t start = 0;
    while (start < metadata.unreadableBlocks.size()) {
      std::size_t comma = metadata.unreadableBlocks.find(',', start);
      if (comma == std::string::npos) {
        comma = metadata.unreadableBlocks.size();
      }
      unreadable.push_back(std::atoi(metadata.unreadableBlocks.substr(start, comma - start).c_str()));
      start = comma + 1;
    }
  }

  for (std::size_t blockIndex = 0; blockIndex < blocks.size(); ++blockIndex) {
    const int absoluteBlock = static_cast<int>(blockIndex);
    if (std::find(unreadable.begin(), unreadable.end(), absoluteBlock) != unreadable.end()) {
      continue;
    }
    if (!writeClassicBlock(static_cast<byte>(absoluteBlock), blocks[blockIndex].data())) {
      Serial.print(F("Restore skip block "));
      Serial.println(absoluteBlock);
    }
  }

  showMessage("Restore done", fileName, "Verify the target", "card manually", 2200);
  return true;
}

std::vector<String> FirmwareApp::listDumpFiles() {
  std::vector<String> result;
  if (!sdReady_) {
    return result;
  }
  File root = SD.open("/");
  if (!root) {
    return result;
  }
  while (true) {
    File entry = root.openNextFile();
    if (!entry) {
      break;
    }
    if (!entry.isDirectory()) {
      String name = entry.name();
      if (name.endsWith(".dump") || name.endsWith(".txt")) {
        result.push_back(name);
      }
    }
    entry.close();
  }
  root.close();
  std::sort(result.begin(), result.end());
  return result;
}

String FirmwareApp::nextAutoDumpName(const CardInfo& info) const {
  String uid = makeSafeFileComponent(info.uidText);
  if (uid.length() == 0) {
    uid = "card";
  }
  return "dump_" + uid + "_" + String(static_cast<unsigned long>(millis())) + ".dump";
}

bool FirmwareApp::writeDumpFile(const String& fileName, const rfid::DumpMetadata& metadata, const std::vector<rfid::DumpBlock>& blocks) {
  File file = SD.open(fileName, FILE_WRITE);
  if (!file) {
    showMessage("File error", "Open failed", fileName, "", 1800);
    return false;
  }
  const std::string serialized = rfid::serializeDump(metadata, blocks);
  file.print(serialized.c_str());
  file.close();
  return true;
}

bool FirmwareApp::readDumpFile(const String& fileName, rfid::DumpMetadata& metadata, std::vector<rfid::DumpBlock>& blocks) {
  File file = SD.open(fileName, FILE_READ);
  if (!file) {
    return false;
  }
  String contents;
  while (file.available()) {
    contents += static_cast<char>(file.read());
  }
  file.close();
  return rfid::parseDump(contents.c_str(), metadata, blocks);
}

void FirmwareApp::haltCard() {
  reader_.PICC_HaltA();
  reader_.PCD_StopCrypto1();
}

void FirmwareApp::showMessage(const String& title, const String& line1, const String& line2, const String& line3, unsigned long holdMs) {
  messageTitle_ = title;
  messageLine1_ = line1;
  messageLine2_ = line2;
  messageLine3_ = line3;
  screen_ = Screen::Message;
  messageUntil_ = holdMs == 0 ? 0 : millis() + holdMs;
  Serial.print(F("MSG "));
  Serial.print(title);
  if (line1.length() > 0) {
    Serial.print(F(" | "));
    Serial.print(line1);
  }
  if (line2.length() > 0) {
    Serial.print(F(" | "));
    Serial.print(line2);
  }
  if (line3.length() > 0) {
    Serial.print(F(" | "));
    Serial.print(line3);
  }
  Serial.println();
}

void FirmwareApp::showCardSnapshot(const CardInfo& info, const String& subtitle) {
  messageTitle_ = "Card found";
  messageLine1_ = info.uidText;
  messageLine2_ = info.typeText;
  messageLine3_ = subtitle;
  screen_ = Screen::Message;
  messageUntil_ = 0;
}

void FirmwareApp::printStatus(const String& prefix, MFRC522::StatusCode status) {
  Serial.print(prefix);
  Serial.print(F(": "));
  Serial.println(reader_.GetStatusCodeName(status));
}

void FirmwareApp::printKey(const MFRC522::MIFARE_Key& key) {
  Serial.print(rfid::bytesToHex(key.keyByte, MFRC522::MF_KEY_SIZE).c_str());
}

bool FirmwareApp::parseHexByteToken(const String& token, byte& value) {
  char* end = nullptr;
  const unsigned long parsed = strtoul(token.c_str(), &end, 16);
  if (end == token.c_str() || parsed > 0xFF) {
    return false;
  }
  value = static_cast<byte>(parsed);
  return true;
}

bool FirmwareApp::parseHexByteList(const std::vector<std::string>& tokens, std::vector<byte>& out) {
  out.clear();
  for (const std::string& token : tokens) {
    byte value = 0;
    if (!parseHexByteToken(String(token.c_str()), value)) {
      return false;
    }
    out.push_back(value);
  }
  return true;
}

String FirmwareApp::joinTokens(const std::vector<String>& tokens, std::size_t fromIndex) {
  String out;
  for (std::size_t i = fromIndex; i < tokens.size(); ++i) {
    if (i > fromIndex) {
      out += " ";
    }
    out += tokens[i];
  }
  return out;
}

String FirmwareApp::formatBytesAsHex(const byte* buffer, std::size_t size, bool spaced) {
  return String(rfid::bytesToHex(buffer, size, spaced).c_str());
}

String FirmwareApp::makeSafeFileComponent(const String& value) {
  String result;
  for (std::size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    if (std::isalnum(static_cast<unsigned char>(c))) {
      result += c;
    }
  }
  return result;
}

String FirmwareApp::trimCopy(const String& value) {
  int start = 0;
  while (start < static_cast<int>(value.length()) && std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }
  int end = static_cast<int>(value.length()) - 1;
  while (end >= start && std::isspace(static_cast<unsigned char>(value[end]))) {
    --end;
  }
  if (end < start) {
    return "";
  }
  return value.substring(start, end + 1);
}
