#pragma once
#include <Arduino.h>


// ESP32
#define PIN_ESP_EN PA0
#define PIN_ESP_BOOT PA1
#define PIN_ESP_TX PA2   
#define PIN_ESP_RX PA3  
static constexpr uint32_t ESP_LINK_BAUD = 115200;

// Sensor
#define PIN_SENS_SCK PB13
#define PIN_SENS_MISO PB14
#define PIN_SENS_MOSI PB15
#define PIN_IMU_CS PA9
#define PIN_BARO_CS PB12
#define PIN_IMU_INT1 PA8
#define PIN_IMU_INT2 PC9
#define PIN_BARO_INT PB11

// Magnetometer
#define PIN_MAG_SDA PB7
#define PIN_MAG_SCL PA15
static constexpr uint8_t MMC5603_ADDR = 0x30;

// Motor Driver
#define PIN_M_INHA PC6 
#define PIN_M_INHB PC7
#define PIN_M_INHC PC8 
#define PIN_M_INLA PC10
#define PIN_M_INLB PC11
#define PIN_M_INLC PC12
#define PIN_M_NSLEEP PA4
#define PIN_M_NFAULT PB5
#define PIN_M_SOA PA6
#define PIN_M_SOB PA7
#define PIN_M_SOC PB1

// Servo
#define PIN_SERVO      PB4

// HMI
#define PIN_STATUS_LED PB6
#define PIN_LED_ARRAY PC4
#define PIN_BUZZER PC5
static constexpr uint16_t LED_ARRAY_COUNT = 4;

// Power
#define PIN_VMON PB0
#define PIN_USB_VBUS PA10
static constexpr float VBAT_DIVIDER = 2.0f;
static constexpr float ADC_VREF = 3.3f;

// 1S LiPo
static constexpr float VBAT_WARN = 3.60f;
static constexpr float VBAT_CRITICAL = 3.40f;