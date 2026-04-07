#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MFRC522.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "app_config.h"
#include "command_parser.h"
#include "dump_format.h"
#include "rfid_model.h"

class FirmwareApp {
 public:
  void begin();
  void tick();

 private:
  enum class Screen {
    Home,
    Inspect,
    ReadBlockSelect,
    ReadBlockResult,
    DumpSave,
    RestorePick,
    Diagnostics,
    About,
    Message,
  };

  struct CardInfo {
    MFRC522::PICC_Type piccType = MFRC522::PICC_TYPE_UNKNOWN;
    rfid::CardFamily family = rfid::CardFamily::Unknown;
    MFRC522::Uid uid{};
    String uidText;
    String typeText;
    bool classic = false;
    bool magicCapable = false;
  };

  struct MenuItem {
    const char* label;
    Screen target;
  };

  struct ButtonState {
    uint8_t pin = 0;
    bool lastRaw = true;
    bool stable = true;
    unsigned long lastChange = 0;
    bool pressedEdge = false;
  };

  static constexpr std::size_t kKnownKeyCount = 8;

  Adafruit_SSD1306 display_{cypher::OLED_WIDTH, cypher::OLED_HEIGHT, &Wire, -1};
  MFRC522 reader_{cypher::PIN_RFID_SS, cypher::PIN_RFID_RST};
  MFRC522::MIFARE_Key knownKeys_[kKnownKeyCount]{};
  MFRC522::MIFARE_Key activeKey_{};

  bool displayReady_ = false;
  bool sdReady_ = false;
  bool readerReady_ = false;
  bool helpShown_ = false;
  bool serialLineReady_ = false;
  bool awaitingCard_ = false;

  Screen screen_ = Screen::Home;
  int selectedMenu_ = 0;
  int menuScroll_ = 0;
  int selectedBlock_ = 4;
  int selectedDumpIndex_ = 0;

  String messageTitle_;
  String messageLine1_;
  String messageLine2_;
  String messageLine3_;
  unsigned long messageUntil_ = 0;

  String serialBuffer_;
  String lastDumpFile_;
  String selectedDumpFile_;
  std::vector<String> dumpFiles_;

  CardInfo currentCard_{};
  bool currentCardValid_ = false;

  ButtonState buttonUp_{cypher::PIN_BUTTON_UP};
  ButtonState buttonDown_{cypher::PIN_BUTTON_DOWN};
  ButtonState buttonSelect_{cypher::PIN_BUTTON_SELECT};

  const MenuItem menuItems_[6] = {
      {"Inspect card", Screen::Inspect},
      {"Read block", Screen::ReadBlockSelect},
      {"Save dump", Screen::DumpSave},
      {"Restore dump", Screen::RestorePick},
      {"Diagnostics", Screen::Diagnostics},
      {"About", Screen::About},
  };

  void setupButtons();
  void setupKeys();
  void setupDisplay();
  void setupReader();
  void setupStorage();

  void render();
  void renderHome();
  void renderInspect();
  void renderReadBlockSelect();
  void renderReadBlockResult();
  void renderDumpSave();
  void renderRestorePick();
  void renderDiagnostics();
  void renderAbout();
  void renderMessage();

  void pollButtons();
  bool updateButton(ButtonState& button);
  bool buttonPressed(ButtonState& button);
  bool buttonPressedEdge(ButtonState& button);

  void pollSerial();
  void processSerialLine(const String& line);

  bool detectCard(CardInfo& info);
  bool refreshCardInfo();
  static rfid::CardFamily classifyFamily(MFRC522::PICC_Type type);
  static bool cardHasClassicOps(MFRC522::PICC_Type type);
  static bool cardCanUseUidBackdoor(MFRC522::PICC_Type type);
  static String piccTypeToText(MFRC522::PICC_Type type);

  bool tryKnownKeyForSector(byte sector, MFRC522::MIFARE_Key& outKey, byte& outAuthCommand);
  bool authenticateBlock(byte block, byte authCommand, const MFRC522::MIFARE_Key& key);
  bool readClassicBlock(byte block, byte* out);
  bool writeClassicBlock(byte block, const byte* data);
  bool readClassicSector(byte sector, std::vector<rfid::DumpBlock>& blocks, String& unreadableMask);
  bool saveDumpToSd(const CardInfo& info);
  bool restoreDumpFromSd(const String& fileName);
  std::vector<String> listDumpFiles();
  String nextAutoDumpName(const CardInfo& info) const;
  bool writeDumpFile(const String& fileName, const rfid::DumpMetadata& metadata, const std::vector<rfid::DumpBlock>& blocks);
  bool readDumpFile(const String& fileName, rfid::DumpMetadata& metadata, std::vector<rfid::DumpBlock>& blocks);

  void haltCard();
  void showMessage(const String& title, const String& line1 = "", const String& line2 = "", const String& line3 = "", unsigned long holdMs = 0);
  void showCardSnapshot(const CardInfo& info, const String& subtitle = "");
  void printStatus(const String& prefix, MFRC522::StatusCode status);
  void printKey(const MFRC522::MIFARE_Key& key);
  static bool parseHexByteToken(const String& token, byte& value);
  static bool parseHexByteList(const std::vector<std::string>& tokens, std::vector<byte>& out);
  static String joinTokens(const std::vector<String>& tokens, std::size_t fromIndex = 0);
  static String formatBytesAsHex(const byte* buffer, std::size_t size, bool spaced = true);
  static String makeSafeFileComponent(const String& value);
  static String trimCopy(const String& value);
};
