#ifndef SENSOR_MEASUREMENT_H
#define SENSOR_MEASUREMENT_H

#include <cstdint>

struct SensorMeasurement {
    float value;
    int64_t timestamp_us;
    bool valid;
    uint32_t sequence_number;
};

#endif