#include <cstdio>
#include <algorithm>
#include <cinttypes>
#include <cstdlib>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "freertos/queue.h"
#include "sensor_measurement.hpp"

QueueHandle_t measurement_q = nullptr;

void sensorTask(void *parameter) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(10);

    uint32_t sequence_count = 0;

    SensorMeasurement measurement;

    while (true) {
        int64_t curr_time = esp_timer_get_time();

        sequence_count++;

        if (sequence_count <= 300) {
            measurement = {2.45f, curr_time, true, sequence_count};
            xQueueOverwrite(measurement_q, &measurement);
        } else if (sequence_count >= 500) {
            measurement = {2.45f, curr_time, true, sequence_count};
            xQueueOverwrite(measurement_q, &measurement);
        }
                
        xTaskDelayUntil(&lastWakeTime, period);
    };
};

enum class SupervisorState {
    NORMAL, 
    DEGRADED, 
    SAFE
};

const char* stateToString(SupervisorState state) {
    switch (state) {
        case SupervisorState::NORMAL:
            return "NORMAL";

        case SupervisorState::DEGRADED:
            return "DEGRADED";

        case SupervisorState::SAFE:
            return "SAFE";
    }

    return "UNKNOWN";
};

void supervisorTask(void *parameter) {
    // Initialize periodic timing
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(20);

    uint32_t count = 0;
    SensorMeasurement from_sensor;
    bool copy_success = false;
    bool sensor_healthy = true;

    SupervisorState state = SupervisorState::NORMAL;
    uint32_t consecutive_healthy = 0;
    uint32_t prev_seq_num = 0;
    bool has_previous_sequence = false;

    while (true)
    {   
        int64_t now_us = esp_timer_get_time();

        // Supervisor work
        count++;

        if (xQueuePeek(measurement_q, &from_sensor, 0) == pdTRUE) {
            copy_success = true;

            if (now_us - from_sensor.timestamp_us <= 30000 && from_sensor.valid) {
                sensor_healthy = true;
            } else {
                sensor_healthy = false;
            };

        } else {
            sensor_healthy = false;
            copy_success = false;
        };

        bool new_measurement = false;

        if (copy_success) {
            if (!has_previous_sequence || 
                from_sensor.sequence_number != prev_seq_num) {
                    new_measurement = true;
                }
        };

        SupervisorState prev_state = state;

        switch (state) {
            case SupervisorState::NORMAL:
                if (!sensor_healthy) {
                    state = SupervisorState::DEGRADED;
                    consecutive_healthy = 0;
                };
                break;

            case SupervisorState::DEGRADED:
                if (sensor_healthy) {

                    if (new_measurement) {
                        consecutive_healthy++;

                        if (consecutive_healthy >= 5) {
                            state = SupervisorState::NORMAL;
                        };
                    };
                    
                } else {
                    consecutive_healthy = 0;
                };
                break;

            case SupervisorState::SAFE:
                break;
        };

        if (state != prev_state) {
            printf("%s -> %s\n", stateToString(prev_state), stateToString(state));
        };

        if (copy_success) {
            prev_seq_num = from_sensor.sequence_number;
            has_previous_sequence = true;
        };
        
        if (count % 50 == 0) {

            if (copy_success && sensor_healthy && from_sensor.valid) {
                printf("data is fresh! sequence number = %" PRIu32 "\n", from_sensor.sequence_number);
            } else {
                printf("sensor unhealthy\n");
            }
        };

        xTaskDelayUntil(&lastWakeTime, period);
    }; 
};

extern "C" void app_main(void)
{   
    measurement_q = xQueueCreate(
        1, 
        sizeof(SensorMeasurement)
    );

    if (measurement_q == nullptr) {
        printf("Failed to create measurement queue!\n");
        return;
    }

    xTaskCreatePinnedToCore(
        sensorTask,
            "Sensor Task",
        4096,
        nullptr,
        2,
        nullptr,
        1
    );

    xTaskCreatePinnedToCore(
        supervisorTask,
        "Supervisor Task",
        4096,
        nullptr,
        3,
        nullptr,
        1
    );
}
