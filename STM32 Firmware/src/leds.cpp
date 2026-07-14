#include "leds.h"
#include "pins.h"

static uint8_t s_array[LED_ARRAY_COUNT][3]; 
static uint8_t s_status[3];

static inline void dwtInit() {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}
static inline uint32_t cycles() { return DWT->CYCCNT; }

static constexpr uint32_t F_CPU_MHZ = 170;
static constexpr uint32_t T0H = (uint32_t)(0.30f * F_CPU_MHZ);
static constexpr uint32_t T1H = (uint32_t)(0.60f * F_CPU_MHZ);
static constexpr uint32_t TTOT = (uint32_t)(1.25f * F_CPU_MHZ);

static void sendBytes(GPIO_TypeDef* port, uint32_t mask,
                      const uint8_t* data, size_t len) {
    noInterrupts();
    for (size_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        for (int8_t bit = 7; bit >= 0; --bit) {
            uint32_t high = (byte & (1 << bit)) ? T1H : T0H;
            uint32_t t0 = cycles();

            port->BSRR = mask; 
            while (cycles() - t0 < high) { }

            port->BSRR = mask << 16;    
            while (cycles() - t0 < TTOT) { }
        }
    }
    interrupts();
    delayMicroseconds(80);   
}

void Leds::begin() {
    dwtInit();
    pinMode(PIN_STATUS_LED, OUTPUT);
    pinMode(PIN_LED_ARRAY, OUTPUT);
    digitalWrite(PIN_STATUS_LED, LOW);
    digitalWrite(PIN_LED_ARRAY, LOW);

    setStatus(0, 0, 0);
    setArrayAll(0, 0, 0);
    showArray();
}

void Leds::setStatus(uint8_t r, uint8_t g, uint8_t b) {
    s_status[0] = g; 
    s_status[1] = r;
    s_status[2] = b;
    sendBytes(digitalPinToPort(PIN_STATUS_LED),
              digitalPinToBitMask(PIN_STATUS_LED), s_status, 3);
}

void Leds::setArray(uint16_t idx, uint8_t r, uint8_t g, uint8_t b) {
    if (idx >= LED_ARRAY_COUNT) return;
    s_array[idx][0] = g;
    s_array[idx][1] = r;
    s_array[idx][2] = b;
}

void Leds::setArrayAll(uint8_t r, uint8_t g, uint8_t b) {
    for (uint16_t i = 0; i < LED_ARRAY_COUNT; i++) setArray(i, r, g, b);
}

void Leds::showArray() {
    sendBytes(digitalPinToPort(PIN_LED_ARRAY), digitalPinToBitMask(PIN_LED_ARRAY), &s_array[0][0], LED_ARRAY_COUNT * 3);
}

void Leds::statusIdle()  { setStatus(0,  0,  40); }
void Leds::statusArmed() { setStatus(0,  60, 0);  }
void Leds::statusWarn()  { setStatus(60, 30, 0);  }
void Leds::statusError() { setStatus(60, 0,  0);  }