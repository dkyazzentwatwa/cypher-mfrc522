#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ESP32-C3 SuperMini pins
#define RST_PIN 10
#define SS_PIN 7
#define SCK_PIN 4
#define MISO_PIN 5
#define MOSI_PIN 6

// Buttons
#define BUTTON_UP 3
#define BUTTON_DOWN 1
#define BUTTON_SELECT 2

// Display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SSD1306_I2C_ADDRESS 0x3C

// Menu settings
#define VISIBLE_MENU_ITEMS 4
#define MENU_START_Y 16
#define MENU_ITEM_HEIGHT 12

// Menu states
enum MenuState {
  STATE_MENU,
  STATE_READ_CARD,
  STATE_READ_BLOCK,
  STATE_WRITE_BLOCK,
  STATE_FORMAT_CARD,
  STATE_ABOUT
};

// Global variables
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
MFRC522 mfrc522(SS_PIN, RST_PIN);
MenuState currentState = STATE_MENU;
int selectedMenuItem = 0;
int topMenuItem = 0;
const int MENU_ITEMS = 5;
bool buttonPressed = false;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 200;

const char* menuItems[] = {
  "Read Card",
  "Read Block",
  "Write Block",
  "Format Card",
  "About"
};

void initDisplay() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, SSD1306_I2C_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  display.display();
  delay(2000);
  display.clearDisplay();
}

void drawBorder() {
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
}

void displayMenu() {
  display.clearDisplay();
  drawBorder();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Draw title
  display.setCursor(4, 4);
  display.println("MFRC522 Menu");
  display.drawLine(0, 14, SCREEN_WIDTH, 14, SSD1306_WHITE);

  // Calculate which items to show
  if (selectedMenuItem >= topMenuItem + VISIBLE_MENU_ITEMS) {
    topMenuItem = selectedMenuItem - VISIBLE_MENU_ITEMS + 1;
  } else if (selectedMenuItem < topMenuItem) {
    topMenuItem = selectedMenuItem;
  }

  // Draw menu items
  for (int i = 0; i < VISIBLE_MENU_ITEMS && (i + topMenuItem) < MENU_ITEMS; i++) {
    int currentIndex = i + topMenuItem;
    display.setCursor(10, MENU_START_Y + (i * MENU_ITEM_HEIGHT));

    if (currentIndex == selectedMenuItem) {
      display.print("> ");
    } else {
      display.print("  ");
    }
    display.println(menuItems[currentIndex]);
  }

  // Draw scroll indicators if needed
  if (topMenuItem > 0) {
    display.fillTriangle(120, 16, 124, 16, 122, 14, SSD1306_WHITE);  // Up arrow
  }
  if (topMenuItem + VISIBLE_MENU_ITEMS < MENU_ITEMS) {
    display.fillTriangle(120, 60, 124, 60, 122, 62, SSD1306_WHITE);  // Down arrow
  }

  display.display();
}

void displayInfo(String title, String info1 = "", String info2 = "", String info3 = "") {
  display.clearDisplay();
  drawBorder();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(4, 4);
  display.println(title);
  display.drawLine(0, 14, SCREEN_WIDTH, 14, SSD1306_WHITE);

  display.setCursor(4, 18);
  display.println(info1);
  display.setCursor(4, 32);
  display.println(info2);
  display.setCursor(4, 50);
  display.println(info3);

  display.display();
}

String readCard() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return "";
  }

  String uidString = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    uidString += (mfrc522.uid.uidByte[i] < 0x10 ? "0" : "") + String(mfrc522.uid.uidByte[i], HEX) + " ";
  }
  return uidString;
}

bool readBlock(byte blockAddr, byte* buffer) {
  MFRC522::StatusCode status;
  byte size = 18;

  status = mfrc522.MIFARE_Read(blockAddr, buffer, &size);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("MIFARE_Read() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }
  return true;
}

void handleReadCard() {
  while (true) {
    displayInfo("Read Card", "Waiting for card...", "SELECT to exit");

    // Check for exit condition
    if (digitalRead(BUTTON_SELECT) == LOW) {
      if (millis() - lastDebounceTime > debounceDelay) {
        lastDebounceTime = millis();
        break;
      }
    }

    // Check for card
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      String uidString = "";
      for (byte i = 0; i < mfrc522.uid.size; i++) {
        uidString += (mfrc522.uid.uidByte[i] < 0x10 ? "0" : "") + String(mfrc522.uid.uidByte[i], HEX) + " ";
      }

      String typeString = mfrc522.PICC_GetTypeName(mfrc522.PICC_GetType(mfrc522.uid.sak));
      
      // Truncate typeString if it's too long
      if (typeString.length() > 14) { // Adjust the length as needed
        typeString = typeString.substring(0, 14) + "..."; // Add ellipsis if truncated
      }

      displayInfo("Card Found", "UID: " + uidString, "Type: " + typeString, "SELECT to exit");

      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();

      delay(1500);  // Show card info for 1.5 seconds
    }
  }

  // Wait for button release
  while (digitalRead(BUTTON_SELECT) == LOW) {
    delay(10);
  }
}

void handleReadBlock() {
  int blockAddr = 0;

  // Block selection UI
  while (true) {
    displayInfo("Select Block", "Block: " + String(blockAddr),
                "UP/DOWN to change", "SELECT to confirm");

    if (digitalRead(BUTTON_UP) == LOW) {
      if (millis() - lastDebounceTime > debounceDelay) {
        lastDebounceTime = millis();
        blockAddr = (blockAddr + 1) % 64;
      }
    }

    if (digitalRead(BUTTON_DOWN) == LOW) {
      if (millis() - lastDebounceTime > debounceDelay) {
        lastDebounceTime = millis();
        blockAddr = (blockAddr - 1 + 64) % 64;
      }
    }

    if (digitalRead(BUTTON_SELECT) == LOW) {
      if (millis() - lastDebounceTime > debounceDelay) {
        lastDebounceTime = millis();
        break;
      }
    }
  }

  while (digitalRead(BUTTON_SELECT) == LOW) {
    delay(10);
  }

  if (blockAddr < 0 || blockAddr > 63) {
    displayInfo("Error", "Invalid block", String(blockAddr));
    delay(2000);
    return;
  }

  while (true) {
    displayInfo("Read Block", "Block " + String(blockAddr),
                "UP to read", "SELECT to exit");

    if (digitalRead(BUTTON_SELECT) == LOW) {
      if (millis() - lastDebounceTime > debounceDelay) {
        lastDebounceTime = millis();
        break;
      }
    }

    if (digitalRead(BUTTON_UP) == LOW) {
      if (millis() - lastDebounceTime > debounceDelay) {
        lastDebounceTime = millis();

        if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
          byte buffer[18];
          bool success = readBlock(blockAddr, buffer);

          if (success) {
            String blockData = "";
            for (byte i = 0; i < 16; i++) {
              blockData += (buffer[i] < 0x10 ? "0" : "") + String(buffer[i], HEX) + " ";
            }
            displayInfo("Block Data", "Block " + String(blockAddr) + ":", blockData, "SELECT to exit");
            delay(2000);
          } else {
            displayInfo("Error", "Failed to read", "block " + String(blockAddr), "SELECT to exit");
            delay(2000);
          }

          mfrc522.PICC_HaltA();
          mfrc522.PCD_StopCrypto1();
        }
      }
    }
  }

  while (digitalRead(BUTTON_SELECT) == LOW) {
    delay(10);
  }
}

void handleButtons() {
  if (millis() - lastDebounceTime < debounceDelay) {
    return;
  }

  if (currentState == STATE_MENU) {
    if (digitalRead(BUTTON_UP) == LOW) {
      selectedMenuItem = (selectedMenuItem - 1 + MENU_ITEMS) % MENU_ITEMS;
      lastDebounceTime = millis();
    }

    if (digitalRead(BUTTON_DOWN) == LOW) {
      selectedMenuItem = (selectedMenuItem + 1) % MENU_ITEMS;
      lastDebounceTime = millis();
    }

    if (digitalRead(BUTTON_SELECT) == LOW) {
      lastDebounceTime = millis();

      switch (selectedMenuItem) {
        case 0:
          handleReadCard();
          break;
        case 1:
          handleReadBlock();
          break;
        case 2:
          displayInfo("Write Block", "Coming soon...", "", "SELECT to exit");
          while (digitalRead(BUTTON_SELECT) == LOW) delay(10);
          delay(1000);
          break;
        case 3:
          displayInfo("Format Card", "Coming soon...", "", "SELECT to exit");
          while (digitalRead(BUTTON_SELECT) == LOW) delay(10);
          delay(1000);
          break;
        case 4:
          displayInfo("About", "MFRC522 Reader",
                      "FW: " + String(mfrc522.PCD_ReadRegister(mfrc522.VersionReg), HEX),
                      "SELECT to exit");
          while (digitalRead(BUTTON_SELECT) == LOW) delay(10);
          delay(2000);
          break;
      }
    }
  }
}
void setup() {
  Serial.begin(115200);
  while (!Serial)
    ;

  // Initialize buttons
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_SELECT, INPUT_PULLUP);

  // Initialize I2C
  Wire.begin(8, 9);
  Wire.setClock(100000);

  // Initialize display
  initDisplay();
  displayInfo("MFRC522 Reader", "Initializing...");

  // Initialize SPI and MFRC522
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  mfrc522.PCD_Init();

  // Check MFRC522
  byte version = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  if (version == 0x00 || version == 0xFF) {
    displayInfo("Error", "Check wiring", "MFRC522 not found");
    while (1)
      ;
  }

  displayInfo("Ready", "Use UP/DOWN to", "navigate menu", "SELECT to choose");
  delay(2000);
}

void loop() {
  if (currentState == STATE_MENU) {
    displayMenu();
  }
  handleButtons();
}