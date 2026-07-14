#include "sensors.h"
#include "pins.h"
#include <SPI.h>
#include <Wire.h>
#include <math.h>

static SPIClass sensorSpi(PIN_SENS_MOSI, PIN_SENS_MISO, PIN_SENS_SCK);
static const SPISettings kSpi(8000000, MSBFIRST, SPI_MODE3);

static TwoWire magWire(PIN_MAG_SDA, PIN_MAG_SCL);

// LSM6DSR
static constexpr uint8_t LSM_WHO_AM_I = 0x0F; 
static constexpr uint8_t LSM_CTRL1_XL = 0x10;
static constexpr uint8_t LSM_CTRL2_G = 0x11;
static constexpr uint8_t LSM_CTRL3_C = 0x12;
static constexpr uint8_t LSM_CTRL6_C = 0x15;
static constexpr uint8_t LSM_CTRL9_XL = 0x18;
static constexpr uint8_t LSM_OUTX_L_G = 0x22;

// BMP581
static constexpr uint8_t BMP_CHIP_ID    = 0x01; 
static constexpr uint8_t BMP_TEMP_DATA  = 0x1D;
static constexpr uint8_t BMP_DSP_IIR    = 0x31;
static constexpr uint8_t BMP_OSR_CFG    = 0x36;
static constexpr uint8_t BMP_ODR_CFG    = 0x37;
static constexpr uint8_t BMP_CMD        = 0x7E;

// MMC5603NJ
static constexpr uint8_t MMC_XOUT0 = 0x00;
static constexpr uint8_t MMC_STATUS1 = 0x18;
static constexpr uint8_t MMC_ODR = 0x1A;
static constexpr uint8_t MMC_CTRL0 = 0x1B;
static constexpr uint8_t MMC_CTRL1 = 0x1C;
static constexpr uint8_t MMC_CTRL2 = 0x1D;
static constexpr uint8_t MMC_PRODUCT_ID = 0x39; 

static constexpr float ACC_LSB = 0.244f / 1000.0f; 
static constexpr float GYRO_LSB = 70.0f  / 1000.0f; 
static constexpr float MAG_LSB = 0.0625f / 1000.0f; 

static bool s_imu = false, s_baro = false, s_mag = false;

static void spiWrite(uint8_t cs, uint8_t reg, uint8_t val) {
    sensorSpi.beginTransaction(kSpi);
    digitalWrite(cs, LOW);
    sensorSpi.transfer(reg & 0x7F);
    sensorSpi.transfer(val);
    digitalWrite(cs, HIGH);
    sensorSpi.endTransaction();
}

static void spiRead(uint8_t cs, uint8_t reg, uint8_t* buf, size_t len) {
    sensorSpi.beginTransaction(kSpi);
    digitalWrite(cs, LOW);
    sensorSpi.transfer(reg | 0x80);
    for (size_t i = 0; i < len; i++) buf[i] = sensorSpi.transfer(0x00);
    digitalWrite(cs, HIGH);
    sensorSpi.endTransaction();
}

static uint8_t spiRead1(uint8_t cs, uint8_t reg) {
    uint8_t v = 0;
    spiRead(cs, reg, &v, 1);
    return v;
}

static void magWrite(uint8_t reg, uint8_t val) {
    magWire.beginTransmission(MMC5603_ADDR);
    magWire.write(reg);
    magWire.write(val);
    magWire.endTransmission();
}

static bool magRead(uint8_t reg, uint8_t* buf, size_t len) {
    magWire.beginTransmission(MMC5603_ADDR);
    magWire.write(reg);
    if (magWire.endTransmission(false) != 0) return false;
    size_t got = magWire.requestFrom((uint8_t)MMC5603_ADDR, (uint8_t)len);
    if (got != len) return false;
    for (size_t i = 0; i < len; i++) buf[i] = magWire.read();
    return true;
}

static inline int16_t le16(const uint8_t* p) {
    return (int16_t)((uint16_t)p[1] << 8 | p[0]);
}

bool Sensors::imuOk()  { return s_imu; }
bool Sensors::baroOk() { return s_baro; }
bool Sensors::magOk()  { return s_mag; }

bool Sensors::begin() {
    pinMode(PIN_IMU_CS,  OUTPUT);
    pinMode(PIN_BARO_CS, OUTPUT);
    digitalWrite(PIN_IMU_CS,  HIGH);
    digitalWrite(PIN_BARO_CS, HIGH);

    pinMode(PIN_IMU_INT1, INPUT);
    pinMode(PIN_IMU_INT2, INPUT);
    pinMode(PIN_BARO_INT, INPUT);

    sensorSpi.begin();
    magWire.begin();
    magWire.setClock(400000);
    delay(20);

    // IMU
    spiWrite(PIN_IMU_CS, LSM_CTRL3_C, 0x01);
    delay(20);
    if (spiRead1(PIN_IMU_CS, LSM_WHO_AM_I) == 0x6B) {
        spiWrite(PIN_IMU_CS, LSM_CTRL3_C, 0x44);
        spiWrite(PIN_IMU_CS, LSM_CTRL9_XL, 0xE2);
        spiWrite(PIN_IMU_CS, LSM_CTRL1_XL, 0x8C);    
        spiWrite(PIN_IMU_CS, LSM_CTRL2_G,  0x8C);  
        spiWrite(PIN_IMU_CS, LSM_CTRL6_C,  0x00);  
        s_imu = true;
    }

    // BARO
    spiWrite(PIN_BARO_CS, BMP_CMD, 0xB6); 
    delay(10);
    if (spiRead1(PIN_BARO_CS, BMP_CHIP_ID) == 0x50) {
        spiWrite(PIN_BARO_CS, BMP_OSR_CFG, 0x4B); 
        spiWrite(PIN_BARO_CS, BMP_DSP_IIR, 0x09); 
        spiWrite(PIN_BARO_CS, BMP_ODR_CFG, 0x59); 
        delay(20);
        s_baro = true;
    }

    // MAG 
    uint8_t id = 0;
    if (magRead(MMC_PRODUCT_ID, &id, 1) && id == 0x10) {
        magWrite(MMC_CTRL1, 0x80); 
        delay(20);
        magWrite(MMC_ODR,   100);
        magWrite(MMC_CTRL0, 0x80); 
        magWrite(MMC_CTRL2, 0x10); 
        magWrite(MMC_CTRL0, 0x20); 
        s_mag = true;
    }

    return s_imu && s_baro && s_mag;
}

bool Sensors::readImu(ImuSample& s) {
    if (!s_imu) return false;
    uint8_t b[12];
    spiRead(PIN_IMU_CS, LSM_OUTX_L_G, b, sizeof(b));

    s.gx = le16(&b[0])  * GYRO_LSB;
    s.gy = le16(&b[2])  * GYRO_LSB;
    s.gz = le16(&b[4])  * GYRO_LSB;
    s.ax = le16(&b[6])  * ACC_LSB;
    s.ay = le16(&b[8])  * ACC_LSB;
    s.az = le16(&b[10]) * ACC_LSB;
    return true;
}

bool Sensors::readBaro(BaroSample& s) {
    if (!s_baro) return false;
    uint8_t b[6];
    spiRead(PIN_BARO_CS, BMP_TEMP_DATA, b, sizeof(b));

    int32_t traw = (int32_t)((uint32_t)b[2] << 16 | (uint32_t)b[1] << 8 | b[0]);
    if (traw & 0x800000) traw |= (int32_t)0xFF000000;
    uint32_t praw = (uint32_t)b[5] << 16 | (uint32_t)b[4] << 8 | b[3];

    s.temperature_c = traw / 65536.0f;
    s.pressure_pa   = praw / 64.0f;
    s.altitude_m    = 44330.0f *
        (1.0f - powf(s.pressure_pa / 101325.0f, 0.1902949f));
    return true;
}

bool Sensors::readMag(MagSample& s) {
    if (!s_mag) return false;
    uint8_t b[6];
    if (!magRead(MMC_XOUT0, b, sizeof(b))) return false;

    auto be16 = [](const uint8_t* p) -> int32_t {
        return (int32_t)((uint32_t)p[0] << 8 | p[1]) - 32768;
    };
    s.mx = be16(&b[0]) * MAG_LSB;
    s.my = be16(&b[2]) * MAG_LSB;
    s.mz = be16(&b[4]) * MAG_LSB;
    return true;
}