#include "battery.h"
#include "motors.h"
#include <Wire.h>
#include <Adafruit_INA219.h>

static Adafruit_INA219 ina(BAT_ADDR);
static bool initialized = false;

BatteryState g_battery = {0.0f, 0.0f, 0.0f, false, 0};
SemaphoreHandle_t g_i2c_mutex = nullptr;

bool battery_init() {
    // Wire already initialized by imu_init(). Do not call Wire.begin() here.
    if (!ina.begin()) {
        return false;
    }
    initialized = true;
    return true;
}

void battery_task_fn(void* param) {
    (void)param;
    for (;;) {
        if (initialized && g_i2c_mutex != nullptr) {
            if (xSemaphoreTake(g_i2c_mutex, pdMS_TO_TICKS(50))) {
                float v  = ina.getBusVoltage_V() + (ina.getShuntVoltage_mV() / 1000.0f);
                float i  = ina.getCurrent_mA();
                float p  = ina.getPower_mW();
                xSemaphoreGive(g_i2c_mutex);

                g_battery.voltage_v    = v;
                g_battery.current_ma   = i;
                g_battery.power_mw     = p;
                g_battery.ok           = (v >= BAT_CUTOFF_V);
                g_battery.timestamp_ms = millis();

                if (v > 0.5f && v < BAT_CUTOFF_V) {
                    // Voltage is measured and below cutoff — emergency stop
                    motors_stop();
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
