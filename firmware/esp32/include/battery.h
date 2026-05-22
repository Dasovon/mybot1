#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#define BAT_ADDR      0x40
#define BAT_WARN_V    10.5f   // 3S LiPo: warn at ~3.5V/cell
#define BAT_CUTOFF_V   9.9f   // 3S LiPo: stop motors at ~3.3V/cell

// Shared battery state — written by battery_task (Core 0), read by loop() (Core 1).
// Individual volatile float reads are atomic on ARM — no mutex needed for reads.
struct BatteryState {
    volatile float   voltage_v;     // bus voltage in V
    volatile float   current_ma;    // current draw in mA
    volatile float   power_mw;      // power in mW
    volatile bool    ok;            // false when voltage < BAT_CUTOFF_V
    volatile uint32_t timestamp_ms;
};
extern BatteryState g_battery;

// Shared I2C mutex — protects Wire bus across Core 0 (battery_task) and Core 1 (loop).
// Must be created in setup() before battery_task is started.
extern SemaphoreHandle_t g_i2c_mutex;

// Called from setup() (Core 1) after Wire is initialized.
bool battery_init();

// FreeRTOS task entry — pinned to Core 0. Reads INA219 every 200 ms.
// Stores results in g_battery. Calls motors_stop() on cutoff voltage.
void battery_task_fn(void* param);
