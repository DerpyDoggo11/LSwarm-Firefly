#include "esp_link.h"
#include "motor.h"
#include "leds.h"

HardwareSerial EspSerial(PIN_ESP_RX, PIN_ESP_TX); 

namespace EspLink {

void begin() {
    pinMode(PIN_ESP_EN, OUTPUT);
    pinMode(PIN_ESP_BOOT, OUTPUT);
    digitalWrite(PIN_ESP_EN, HIGH); 
    digitalWrite(PIN_ESP_BOOT, HIGH); 

    EspSerial.begin(ESP_LINK_BAUD);
}

void reset() {
    digitalWrite(PIN_ESP_BOOT, HIGH);
    digitalWrite(PIN_ESP_EN, LOW);
    delay(50);
    digitalWrite(PIN_ESP_EN, HIGH);
    delay(200);
}

void enterBootloader() {
    digitalWrite(PIN_ESP_BOOT, LOW); 
    digitalWrite(PIN_ESP_EN, LOW);
    delay(50);
    digitalWrite(PIN_ESP_EN, HIGH);    
    delay(150);                    
    digitalWrite(PIN_ESP_BOOT, HIGH);
}

void hold() { digitalWrite(PIN_ESP_EN, LOW); }

[[noreturn]] void passthrough() {
    Motor::disable();
    Leds::setStatus(0, 0, 60); 

    enterBootloader();               

    for (;;) {
        while (Serial.available() && EspSerial.availableForWrite())
            EspSerial.write((uint8_t)Serial.read());
        while (EspSerial.available() && Serial.availableForWrite())
            Serial.write((uint8_t)EspSerial.read());
    }
}

}