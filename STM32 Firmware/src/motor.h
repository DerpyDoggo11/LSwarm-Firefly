#pragma once
#include <Arduino.h>

namespace Motor {

static constexpr uint32_t PWM_FREQ_HZ = 20000;
static constexpr uint16_t DEADTIME_TICKS = 40;

void begin();

void enable();
void disable();
bool enabled();

bool faulted();
void clearFault();

void setPhaseDuty(float a, float b, float c);

void coast(); 
void brake();


void setSVPWM(float angle_rad, float amplitude);

void openLoopSpin(float elec_hz, float amplitude, float dt_s);

struct Currents { float a, b, c; }; 
Currents readCurrents();

}