#pragma once
#include <Arduino.h>

struct ImuSample  { float ax, ay, az; float gx, gy, gz; };
struct BaroSample { float pressure_pa, temperature_c, altitude_m; };
struct MagSample  { float mx, my, mz; };

namespace Sensors {
    bool begin();    
    bool imuOk();
    bool baroOk();
    bool magOk();

    bool readImu(ImuSample& s);
    bool readBaro(BaroSample& s);
    bool readMag(MagSample& s);
}