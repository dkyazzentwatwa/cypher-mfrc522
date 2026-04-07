#pragma once

#include <cstdint>

namespace cypher {

constexpr std::uint8_t PIN_RFID_RST = 21;
constexpr std::uint8_t PIN_RFID_SS = 7;
constexpr std::uint8_t PIN_SPI_SCK = 4;
constexpr std::uint8_t PIN_SPI_MISO = 5;
constexpr std::uint8_t PIN_SPI_MOSI = 6;

constexpr std::uint8_t PIN_OLED_SDA = 8;
constexpr std::uint8_t PIN_OLED_SCL = 9;

constexpr std::uint8_t PIN_SD_CS = 10;

constexpr std::uint8_t PIN_BUTTON_UP = 3;
constexpr std::uint8_t PIN_BUTTON_DOWN = 1;
constexpr std::uint8_t PIN_BUTTON_SELECT = 2;

constexpr std::uint8_t OLED_I2C_ADDRESS = 0x3C;
constexpr std::uint8_t OLED_WIDTH = 128;
constexpr std::uint8_t OLED_HEIGHT = 64;

constexpr unsigned long BUTTON_DEBOUNCE_MS = 175;
constexpr unsigned long SERIAL_IDLE_REFRESH_MS = 1500;
constexpr unsigned long CARD_POLL_MS = 120;

}  // namespace cypher
