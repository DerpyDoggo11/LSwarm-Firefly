#include <Arduino.h>
#include "pins.h"
#include "sensors.h"
#include "motor.h"
#include "leds.h"
#include "io.h"
#include "esp_link.h"

static bool  g_armed = false;
static float g_vbat  = 0.0f;
static bool  g_sensorsOk = false;

static bool  g_spinning  = false;
static float g_spinHz    = 0.0f;
static float g_spinAmp   = 0.0f;

static void safeShutdown(const char* why) {
    g_armed    = false;
    g_spinning = false;
    Motor::disable();
    Leds::statusError();
    Buzzer::beep(400);
    Serial.printf("!! DISARMED: %s\n", why);
}

static void handleCommand(char* cmd) {
    if (!strcmp(cmd, "help")) {
        Serial.println("arm | disarm | spin <elec_hz> <amp 0-1> | stop |");
        Serial.println("duty <a> <b> <c> | servo <us> | stat | imu | mag |");
        Serial.println("baro | fault | esp_reset | esp_boot | esp_bridge");
    }
    else if (!strcmp(cmd, "arm")) {
        if (!g_sensorsOk)                 { Serial.println("refused: sensors failed"); return; }
        if (Motor::faulted())             { Serial.println("refused: DRV8311 fault"); return; }
        if (g_vbat < VBAT_CRITICAL && !Power::usbPresent()) {
            Serial.println("refused: battery low"); return;
        }
        Motor::enable();
        g_armed = true;
        Leds::statusArmed();
        Buzzer::beep(120);
        Serial.println("ARMED");
    }
    else if (!strcmp(cmd, "disarm") || !strcmp(cmd, "stop")) {
        g_armed = g_spinning = false;
        Motor::disable();
        Leds::statusIdle();
        Serial.println("disarmed");
    }
    else if (!strncmp(cmd, "spin ", 5)) {
        float hz, amp;
        if (sscanf(cmd + 5, "%f %f", &hz, &amp) == 2) {
            if (!g_armed) { Serial.println("arm first"); return; }
            g_spinHz   = hz;
            g_spinAmp  = constrain(amp, 0.0f, 0.6f);
            g_spinning = true;
            Serial.printf("open-loop spin: %.1f Hz elec, amp %.2f\n", g_spinHz, g_spinAmp);
        }
    }
    else if (!strncmp(cmd, "duty ", 5)) {
        float a, b, c;
        if (sscanf(cmd + 5, "%f %f %f", &a, &b, &c) == 3) {
            g_spinning = false;
            Motor::setPhaseDuty(a, b, c);
            Serial.printf("duty %.2f %.2f %.2f\n", a, b, c);
        }
    }
    else if (!strncmp(cmd, "servo ", 6)) {
        int us = atoi(cmd + 6);
        ServoOut::setMicros((uint16_t)us);
        Serial.printf("servo %d us\n", us);
    }
    else if (!strcmp(cmd, "stat")) {
        Serial.printf("vbat %.2f V  usb %d  armed %d  fault %d\n", g_vbat, (int)Power::usbPresent(), (int)g_armed, (int)Motor::faulted());
        Serial.printf("imu %d  baro %d  mag %d\n", (int)Sensors::imuOk(), (int)Sensors::baroOk(), (int)Sensors::magOk());
        Motor::Currents c = Motor::readCurrents();
        Serial.printf("isense %.3f %.3f %.3f V\n", c.a, c.b, c.c);
    }
    else if (!strcmp(cmd, "imu")) {
        ImuSample s;
        if (Sensors::readImu(s))
            Serial.printf("acc %.2f %.2f %.2f g | gyro %.1f %.1f %.1f dps\n", s.ax, s.ay, s.az, s.gx, s.gy, s.gz);
        else Serial.println("imu not present");
    }
    else if (!strcmp(cmd, "baro")) {
        BaroSample s;
        if (Sensors::readBaro(s))
            Serial.printf("%.1f Pa  %.1f C  alt %.1f m\n", s.pressure_pa, s.temperature_c, s.altitude_m);
        else Serial.println("baro not present");
    }
    else if (!strcmp(cmd, "mag")) {
        MagSample s;
        if (Sensors::readMag(s))
            Serial.printf("mag %.3f %.3f %.3f G\n", s.mx, s.my, s.mz);
        else Serial.println("mag not present");
    }
    else if (!strcmp(cmd, "fault")) {
        Serial.printf("nFAULT %s\n", Motor::faulted() ? "ASSERTED" : "clear");
        Motor::clearFault();
        Serial.println("fault latch cleared");
    }
    else if (!strcmp(cmd, "esp_reset")) { EspLink::reset(); Serial.println("esp reset"); }
    else if (!strcmp(cmd, "esp_boot"))  { EspLink::enterBootloader(); Serial.println("esp in bootloader"); }
    else if (!strcmp(cmd, "esp_bridge")) {
        Serial.println("bridge mode: ESP is in the ROM bootloader.");
        Serial.println("flash with --before no_reset --after no_reset");
        Serial.flush();
        EspLink::passthrough(); 
    }
    else if (cmd[0]) {
        Serial.println("? try 'help'");
    }
}

void setup() {
    Serial.begin(115200);

    Leds::begin();
    Leds::statusWarn();
    Buzzer::begin();
    Power::begin();
    ServoOut::begin();

    Motor::begin(); 
    EspLink::begin();

    g_sensorsOk = Sensors::begin();

    uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 2000) { }

    Serial.println("\n=== STM32G473 flight controller ===");
    Serial.printf("IMU  (LSM6DSR) : %s\n", Sensors::imuOk()  ? "ok" : "FAIL");
    Serial.printf("BARO (BMP581)  : %s\n", Sensors::baroOk() ? "ok" : "FAIL");
    Serial.printf("MAG  (MMC5603) : %s\n", Sensors::magOk()  ? "ok" : "FAIL");
    Serial.printf("DRV8311 fault  : %s\n", Motor::faulted() ? "ASSERTED" : "clear");
    Serial.printf("VBAT           : %.2f V\n", Power::batteryVolts());
    Serial.println("type 'help'; 'esp_bridge' to flash the ESP32-C3");

    Leds::statusIdle();
    Buzzer::beep(60);
}

void loop() {
    static char    line[64];
    static uint8_t n = 0;
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            if (n) { line[n] = '\0'; handleCommand(line); n = 0; }
        } else if (n < sizeof(line) - 1) {
            line[n++] = c;
        }
    }

    while (EspSerial.available()) Serial.write((char)EspSerial.read());

    static uint32_t nextCtl = 0;
    uint32_t now = micros();
    if ((int32_t)(now - nextCtl) >= 0) {
        nextCtl = now + 200;

        if (Motor::enabled() && Motor::faulted()) {
            safeShutdown("DRV8311 nFAULT");
        }
        else if (g_armed && g_spinning) {
            Motor::openLoopSpin(g_spinHz, g_spinAmp, 0.0002f);
        }
    }

    static uint32_t tHk = 0;
    if (millis() - tHk >= 200) {
        tHk = millis();
        g_vbat = Power::batteryVolts();

        if (!g_sensorsOk)                             Leds::statusError();
        else if (g_vbat < VBAT_CRITICAL)              Leds::statusError();
        else if (g_vbat < VBAT_WARN)                  Leds::statusWarn();
        else if (g_armed)                             Leds::statusArmed();
        else                                          Leds::statusIdle();

        if (g_armed && g_vbat < VBAT_CRITICAL && !Power::usbPresent()) {
            safeShutdown("low voltage");
        }
    }
}