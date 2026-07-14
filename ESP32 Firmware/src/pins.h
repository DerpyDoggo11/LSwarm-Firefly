#pragma once
#include <Arduino.h>

// UWB 
static constexpr int PIN_DW_WAKEUP = 1;
static constexpr int PIN_DW_IRQ = 3;
static constexpr int PIN_DW_MISO = 4;
static constexpr int PIN_DW_MOSI = 5;
static constexpr int PIN_DW_CLK = 6;
static constexpr int PIN_DW_CS = 7;
static constexpr int PIN_DW_RST = 10;

// Magnetometer
static constexpr int PIN_MAG_SDA = 18;
static constexpr int PIN_MAG_SCL = 19;
static constexpr uint8_t MMC5603_ADDR = 0x30;

// STM32 Link
static constexpr int PIN_LINK_RX = 20;
static constexpr int PIN_LINK_TX = 21;
static constexpr uint32_t LINK_BAUD = 115200;