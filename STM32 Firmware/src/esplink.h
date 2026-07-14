#pragma once
#include <Arduino.h>
#include "pins.h"

namespace EspLink {

void begin();

void reset(); 
void enterBootloader();  
void hold(); 

[[noreturn]] void passthrough();

} 

extern HardwareSerial EspSerial;