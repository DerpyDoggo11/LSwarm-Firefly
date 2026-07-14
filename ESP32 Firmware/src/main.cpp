#include <SPI.h>
#include <Wire.h>
#include "pins.h"

static SPIClass dwSpi(FSPI);
static bool     s_uwbOk = false;
static bool     s_magOk = false;

static volatile bool s_irq = false;
static void IRAM_ATTR dwIsr() { s_irq = true; }

static constexpr uint8_t MMC_XOUT0 = 0x00;
static constexpr uint8_t MMC_ODR = 0x1A;
static constexpr uint8_t MMC_CTRL0  = 0x1B;
static constexpr uint8_t MMC_CTRL1  = 0x1C;
static constexpr uint8_t MMC_CTRL2 = 0x1D;
static constexpr uint8_t MMC_PRODUCT_ID = 0x39; 
static constexpr float   MAG_LSB = 0.0625f / 1000.0f;

static void dwHardReset() {
    pinMode(PIN_DW_RST, OUTPUT);
    digitalWrite(PIN_DW_RST, LOW);
    delay(2);
    pinMode(PIN_DW_RST, INPUT);  
    delay(5);
}

static uint16_t dwHeader(uint8_t file, uint16_t offset, bool write) {
    uint16_t h = (uint16_t)((file & 0x1F) << 9) | (uint16_t)((offset & 0x7F) << 2);
    if (write) h |= 0x8000;
    return h;
}

static uint32_t dwReadReg32(uint8_t file, uint16_t offset) {
    uint16_t hdr = dwHeader(file, offset, false);
    uint8_t  b[4] = {0, 0, 0, 0};

    dwSpi.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
    digitalWrite(PIN_DW_CS, LOW);
    dwSpi.transfer((uint8_t)(hdr >> 8));
    dwSpi.transfer((uint8_t)(hdr & 0xFF));
    for (int i = 0; i < 4; i++) b[i] = dwSpi.transfer(0x00);
    digitalWrite(PIN_DW_CS, HIGH);
    dwSpi.endTransaction();

    return (uint32_t)b[3] << 24 | (uint32_t)b[2] << 16 | (uint32_t)b[1] << 8  | (uint32_t)b[0];
}

static bool uwbBegin() {
    pinMode(PIN_DW_CS, OUTPUT);
    digitalWrite(PIN_DW_CS, HIGH);
    pinMode(PIN_DW_WAKEUP, OUTPUT);
    digitalWrite(PIN_DW_WAKEUP, LOW);
    pinMode(PIN_DW_IRQ, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_DW_IRQ), dwIsr, RISING);

    dwSpi.begin(PIN_DW_CLK, PIN_DW_MISO, PIN_DW_MOSI, PIN_DW_CS);
    dwHardReset();

    uint32_t devid = dwReadReg32(0x00, 0x00);
    Serial.printf("S,uwb devid=0x%08X\n", (unsigned int)devid);

    return (devid & 0xFFFF0000u) == 0xDECA0000u;
}

static void magWrite(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(MMC5603_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

static bool magRead(uint8_t reg, uint8_t* buf, size_t len) {
    Wire.beginTransmission(MMC5603_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)MMC5603_ADDR, (int)len) != (int)len) return false;
    for (size_t i = 0; i < len; i++) buf[i] = Wire.read();
    return true;
}

static bool magBegin() {
    Wire.begin(PIN_MAG_SDA, PIN_MAG_SCL, 400000);

    uint8_t id = 0;
    if (!magRead(MMC_PRODUCT_ID, &id, 1) || id != 0x10) return false;

    magWrite(MMC_CTRL1, 0x80);
    delay(20);
    magWrite(MMC_ODR,   100); 
    magWrite(MMC_CTRL0, 0x80); 
    magWrite(MMC_CTRL2, 0x10); 
    magWrite(MMC_CTRL0, 0x20); 
    return true;
}

static bool magSample(float& mx, float& my, float& mz) {
    uint8_t b[6];
    if (!magRead(MMC_XOUT0, b, sizeof(b))) return false;

    auto be16 = [](const uint8_t* p) -> int32_t {
        return (int32_t)((uint32_t)p[0] << 8 | p[1]) - 32768;
    };
    mx = be16(&b[0]) * MAG_LSB;
    my = be16(&b[2]) * MAG_LSB;
    mz = be16(&b[4]) * MAG_LSB;
    return true;
}

void setup() {
    Serial.begin(LINK_BAUD, SERIAL_8N1, PIN_LINK_RX, PIN_LINK_TX);
    delay(50);
    Serial.println("S,esp32-c3 boot");

    s_uwbOk = uwbBegin();
    s_magOk = magBegin();

    Serial.printf("S,uwb=%d mag=%d\n", (int)s_uwbOk, (int)s_magOk);
}

void loop() {
    static char    rxbuf[64];
    static uint8_t rxlen = 0;
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            if (rxlen) {
                rxbuf[rxlen] = '\0';
                if (rxbuf[0] == 'R') {
                    s_uwbOk = uwbBegin();
                    Serial.printf("S,uwb reset -> %d\n", (int)s_uwbOk);
                } else if (rxbuf[0] == '?') {
                    Serial.printf("S,uwb=%d mag=%d heap=%u\n",
                                  (int)s_uwbOk, (int)s_magOk,
                                  (unsigned)ESP.getFreeHeap());
                }
                rxlen = 0;
            }
        } else if (rxlen < sizeof(rxbuf) - 1) {
            rxbuf[rxlen++] = c;
        }
    }

    if (s_irq) {
        s_irq = false;
    }

    static uint32_t tMag = 0;
    if (s_magOk && millis() - tMag >= 20) {
        tMag = millis();
        float mx, my, mz;
        if (magSample(mx, my, mz)) {
            Serial.printf("M,%.4f,%.4f,%.4f\n", mx, my, mz);
        }
    }

    static uint32_t tPos = 0;
    if (s_uwbOk && millis() - tPos >= 100) {
        tPos = millis();
    }
}