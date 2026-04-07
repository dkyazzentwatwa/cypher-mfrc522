#pragma once

#include <cstdint>

namespace cypher {

// XIAO ESP32C3 pin map:
// I2C uses D4/D5, SPI uses D8/D9/D10, and the UART pads are D6/D7.
// Buttons and SD chip select are placed on the remaining exposed GPIOs.
constexpr std::uint8_t PIN_RFID_RST = 21;   // D6 / GPIO21
constexpr std::uint8_t PIN_RFID_SS = 20;    // D7 / GPIO20
constexpr std::uint8_t PIN_SPI_SCK = 8;     // D8 / GPIO8
constexpr std::uint8_t PIN_SPI_MISO = 9;    // D9 / GPIO9
constexpr std::uint8_t PIN_SPI_MOSI = 10;   // D10 / GPIO10

constexpr std::uint8_t PIN_OLED_SDA = 6;    // D4 / GPIO6
constexpr std::uint8_t PIN_OLED_SCL = 7;    // D5 / GPIO7

constexpr std::uint8_t PIN_SD_CS = 2;       // D0 / GPIO2

constexpr std::uint8_t PIN_BUTTON_UP = 3;   // D1 / GPIO3
constexpr std::uint8_t PIN_BUTTON_DOWN = 4; // D2 / GPIO4
constexpr std::uint8_t PIN_BUTTON_SELECT = 5; // D3 / GPIO5

constexpr std::uint8_t OLED_I2C_ADDRESS = 0x3C;
constexpr std::uint8_t OLED_WIDTH = 128;
constexpr std::uint8_t OLED_HEIGHT = 64;

constexpr unsigned long BUTTON_DEBOUNCE_MS = 175;
constexpr unsigned long SERIAL_IDLE_REFRESH_MS = 1500;
constexpr unsigned long CARD_POLL_MS = 120;

}  // namespace cypher
