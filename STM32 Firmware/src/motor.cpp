#include "motor.h"
#include "pins.h"
#include <math.h>

static HardwareTimer* s_tim  = nullptr;
static uint32_t s_period = 0; 
static bool s_enabled = false;
static float s_angle  = 0.0f;

void Motor::begin() {
    pinMode(PIN_M_NSLEEP, OUTPUT);
    digitalWrite(PIN_M_NSLEEP, LOW); 
    s_enabled = false;

    pinMode(PIN_M_NFAULT, INPUT_PULLUP); 

    pinMode(PIN_M_SOA, INPUT_ANALOG);
    pinMode(PIN_M_SOB, INPUT_ANALOG);
    pinMode(PIN_M_SOC, INPUT_ANALOG);

    s_tim = new HardwareTimer(TIM8);

    s_tim->setMode(1, TIMER_OUTPUT_COMPARE_PWM1, PIN_M_INHA);
    s_tim->setMode(1, TIMER_OUTPUT_COMPARE_PWM1, PIN_M_INLA);
    s_tim->setMode(2, TIMER_OUTPUT_COMPARE_PWM1, PIN_M_INHB);
    s_tim->setMode(2, TIMER_OUTPUT_COMPARE_PWM1, PIN_M_INLB);
    s_tim->setMode(3, TIMER_OUTPUT_COMPARE_PWM1, PIN_M_INHC);
    s_tim->setMode(3, TIMER_OUTPUT_COMPARE_PWM1, PIN_M_INLC);

    s_tim->setOverflow(PWM_FREQ_HZ * 2, HERTZ_FORMAT);
    s_period = s_tim->getOverflow(TICK_FORMAT);

    TIM_HandleTypeDef* h = s_tim->getHandle();

    h->Instance->CR1 &= ~TIM_CR1_CMS;
    h->Instance->CR1 |= TIM_CR1_CMS_0;

    TIM_BreakDeadTimeConfigTypeDef bdt = {};
    bdt.OffStateRunMode  = TIM_OSSR_ENABLE;
    bdt.OffStateIDLEMode = TIM_OSSI_ENABLE;
    bdt.LockLevel = TIM_LOCKLEVEL_OFF;
    bdt.DeadTime = DEADTIME_TICKS;
    bdt.BreakState = TIM_BREAK_DISABLE;
    bdt.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
    bdt.BreakFilter = 0;
    bdt.Break2State = TIM_BREAK2_DISABLE;
    bdt.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
    HAL_TIMEx_ConfigBreakDeadTime(h, &bdt);

    coast();
    s_tim->resume();
}

void Motor::enable() {
    coast();          
    digitalWrite(PIN_M_NSLEEP, HIGH);
    delay(2);          
    s_enabled = true;
}

void Motor::disable() {
    coast();
    digitalWrite(PIN_M_NSLEEP, LOW);
    s_enabled = false;
}

bool Motor::enabled()  { return s_enabled; }
bool Motor::faulted()  { return digitalRead(PIN_M_NFAULT) == LOW; }

void Motor::clearFault() {
    digitalWrite(PIN_M_NSLEEP, LOW);
    delay(2);                           
    digitalWrite(PIN_M_NSLEEP, HIGH);
    delay(2);
}

void Motor::setPhaseDuty(float a, float b, float c) {
    if (!s_enabled) { a = b = c = 0.0f; }
    a = constrain(a, 0.0f, 0.98f);
    b = constrain(b, 0.0f, 0.98f);       
    c = constrain(c, 0.0f, 0.98f);

    s_tim->setCaptureCompare(1, (uint32_t)(a * s_period), TICK_COMPARE_FORMAT);
    s_tim->setCaptureCompare(2, (uint32_t)(b * s_period), TICK_COMPARE_FORMAT);
    s_tim->setCaptureCompare(3, (uint32_t)(c * s_period), TICK_COMPARE_FORMAT);
}

void Motor::coast() {
    s_tim->setCaptureCompare(1, 0u, TICK_COMPARE_FORMAT);
    s_tim->setCaptureCompare(2, 0u, TICK_COMPARE_FORMAT);
    s_tim->setCaptureCompare(3, 0u, TICK_COMPARE_FORMAT);
}

void Motor::brake() {
    coast();
}

void Motor::setSVPWM(float angle_rad, float amplitude) {
    amplitude = constrain(amplitude, 0.0f, 1.0f);

    const float k = 2.0f * PI / 3.0f;
    float a = sinf(angle_rad);
    float b = sinf(angle_rad - k);
    float c = sinf(angle_rad + k);

    float vmax = fmaxf(a, fmaxf(b, c));
    float vmin = fminf(a, fminf(b, c));
    float com  = (vmax + vmin) * 0.5f;

    a = (a - com) * amplitude * 0.5f + 0.5f;
    b = (b - com) * amplitude * 0.5f + 0.5f;
    c = (c - com) * amplitude * 0.5f + 0.5f;

    setPhaseDuty(a, b, c);
}

void Motor::openLoopSpin(float elec_hz, float amplitude, float dt_s) {
    s_angle += 2.0f * PI * elec_hz * dt_s;
    while (s_angle > 2.0f * PI) s_angle -= 2.0f * PI;
    while (s_angle < 0.0f)      s_angle += 2.0f * PI;
    setSVPWM(s_angle, amplitude);
}

Motor::Currents Motor::readCurrents() {
    Currents c;
    c.a = analogRead(PIN_M_SOA) / 4095.0f * ADC_VREF;
    c.b = analogRead(PIN_M_SOB) / 4095.0f * ADC_VREF;
    c.c = analogRead(PIN_M_SOC) / 4095.0f * ADC_VREF;
    return c;
}