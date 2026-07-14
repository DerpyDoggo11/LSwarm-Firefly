#pragma once
#include <Arduino.h>
#include <Servo.h>
#include "pins.h"

namespace ServoOut {

inline Servo s_servo;

inline void begin() {
    s_servo.attach(PIN_SERVO, 1000, 2000);
    s_servo.writeMicroseconds(1500);
}

inline void setMicros(uint16_t us) {
    s_servo.writeMicroseconds(constrain(us, 1000, 2000));
}

inline void set(float v) {
    v = constrain(v, -1.0f, 1.0f);
    setMicros((uint16_t)(1500.0f + v * 500.0f));
}

} 

namespace Buzzer {

inline void begin() {
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
}

inline void beep(uint16_t ms = 80, uint16_t freq = 2700) {
    tone(PIN_BUZZER, freq, ms);
}

inline void off() { noTone(PIN_BUZZER); }

}

namespace Power {

inline void begin() {
    analogReadResolution(12);
    pinMode(PIN_VMON, INPUT_ANALOG);
    pinMode(PIN_USB_VBUS, INPUT);
}

inline bool usbPresent() { return digitalRead(PIN_USB_VBUS) == HIGH; }

inline float batteryVolts() {
    uint32_t acc = 0;
    for (uint8_t i = 0; i < 16; i++) acc += analogRead(PIN_VMON);
    return (acc / 16.0f / 4095.0f) * ADC_VREF * VBAT_DIVIDER;
}

}