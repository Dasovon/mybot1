#include <Arduino.h>
#include "motors.h"
#include "encoders.h"
#include "pid.h"
#include "imu.h"
#include "battery.h"
#include "microros.h"

// ---------------------------------------------------------------------------
// Robot physical constants (validated hardware values — do not change)
// ---------------------------------------------------------------------------
static constexpr float WHEEL_SEP   = 0.177f;   // m, center-to-center
static constexpr float WHEEL_RAD   = 0.03414f; // m (measured: 68.27mm dia)
static constexpr float ENC_CPR_F   = 1010.0f;  // counts per wheel revolution
static constexpr float TWO_PI_F    = 2.0f * (float)M_PI;

// ---------------------------------------------------------------------------
// Watchdog — motors stop if no cmd_vel received within this window
// ---------------------------------------------------------------------------
static constexpr uint32_t WATCHDOG_MS = 500;

// ---------------------------------------------------------------------------
// PID controllers — one per wheel, gains require on-hardware tuning
// ---------------------------------------------------------------------------
static PIDController pid_right(PID_KP_DEFAULT, PID_KI_DEFAULT, PID_KD_DEFAULT, 0.01f);
static PIDController pid_left (PID_KP_DEFAULT, PID_KI_DEFAULT, PID_KD_DEFAULT, 0.01f);

// ---------------------------------------------------------------------------
// Odometry state
// ---------------------------------------------------------------------------
static float odom_x     = 0.0f;
static float odom_y     = 0.0f;
static float odom_theta = 0.0f;
static float vel_linear = 0.0f;
static float vel_angular= 0.0f;

// ---------------------------------------------------------------------------
// Encoder tracking
// ---------------------------------------------------------------------------
static long prev_ticks_r = 0;
static long prev_ticks_l = 0;

// ---------------------------------------------------------------------------
// Loop timing
// ---------------------------------------------------------------------------
static uint32_t last_control_ms  = 0;  // 100 Hz
static uint32_t last_pub_ms      = 0;  // 30 Hz  (odom + IMU)
static uint32_t last_bat_pub_ms  = 0;  // 1 Hz   (battery micro-ROS)

// ---------------------------------------------------------------------------
// setup
// ---------------------------------------------------------------------------
void setup() {
    // I2C mutex — must be created before battery_task is started
    g_i2c_mutex = xSemaphoreCreateMutex();

    motors_init();
    encoders_init();

    if (!imu_init()) { }
    if (!battery_init()) { }

    xTaskCreatePinnedToCore(battery_task_fn, "battery", 4096, nullptr, 2, nullptr, 0);

    microros_init();

    uint32_t now = millis();
    last_control_ms = now;
    last_pub_ms     = now;
    last_bat_pub_ms = now;
}

// ---------------------------------------------------------------------------
// loop — runs on Core 1
// ---------------------------------------------------------------------------
void loop() {
    // Turn off the Lonely Binary RGB LED (GPIO 48, WS2812) on the first loop
    // iteration. neopixelWrite() uses the RMT peripheral which is not safe to
    // call at the top of setup() — the DMA callback hasn't fired yet and the
    // call blocks indefinitely. By the first loop() iteration the framework is
    // fully up and the call completes in a few microseconds.
    static bool led_off = false;
    if (!led_off) {
        neopixelWrite(48, 0, 0, 0);
        led_off = true;
    }

    uint32_t now = millis();

    // -----------------------------------------------------------------------
    // 100 Hz — PID + odometry integration
    // -----------------------------------------------------------------------
    if (now - last_control_ms >= 10) {
        float dt = (now - last_control_ms) / 1000.0f;
        last_control_ms = now;

        if (now - microros_last_cmd_ms() > WATCHDOG_MS) {
            // No command received in time — safe stop and reset integrators
            motors_stop();
            pid_right.reset();
            pid_left.reset();
            vel_linear  = 0.0f;
            vel_angular = 0.0f;
        } else {
            long ticks_r = encoders_get_right();
            long ticks_l = encoders_get_left();
            long delta_r = ticks_r - prev_ticks_r;
            long delta_l = ticks_l - prev_ticks_l;
            prev_ticks_r = ticks_r;
            prev_ticks_l = ticks_l;

            // Measured wheel velocity (rad/s)
            float meas_r = (float)delta_r / ENC_CPR_F * TWO_PI_F / dt;
            float meas_l = (float)delta_l / ENC_CPR_F * TWO_PI_F / dt;

            // Target from latest cmd_vel
            float cmd_v = microros_cmd_linear();
            float cmd_w = microros_cmd_angular();
            float tgt_r = (cmd_v + cmd_w * WHEEL_SEP / 2.0f) / WHEEL_RAD;
            float tgt_l = (cmd_v - cmd_w * WHEEL_SEP / 2.0f) / WHEEL_RAD;

            // PID output in rad/s, mapped to [-1, 1] duty for motors.
            // Apply deadband floor: gearbox stiction requires MOTOR_MIN_DUTY to move.
            float out_r = pid_right.compute(tgt_r, meas_r);
            float out_l = pid_left.compute(tgt_l, meas_l);
            auto apply_floor = [](float out, float tgt) -> float {
                float duty = out / MOTOR_MAX_RAD_S;
                if (fabsf(tgt) < 0.01f) return duty;  // coast when target is zero
                // Floor only in the target direction — reverse floor caused hard
                // oscillation when PID applied small corrections against motion.
                if (tgt > 0.0f && duty > 0.0f && duty < MOTOR_MIN_DUTY)  return MOTOR_MIN_DUTY;
                if (tgt < 0.0f && duty < 0.0f && duty > -MOTOR_MIN_DUTY) return -MOTOR_MIN_DUTY;
                return duty;
            };
            motors_set_duty(apply_floor(out_r, tgt_r), apply_floor(out_l, tgt_l));

            // Odometry integration (mid-point rule)
            float d_r  = (float)delta_r / ENC_CPR_F * TWO_PI_F * WHEEL_RAD;
            float d_l  = (float)delta_l / ENC_CPR_F * TWO_PI_F * WHEEL_RAD;
            float d_c  = (d_r + d_l) / 2.0f;
            float d_th = (d_r - d_l) / WHEEL_SEP;
            odom_x     += d_c * cosf(odom_theta + d_th / 2.0f);
            odom_y     += d_c * sinf(odom_theta + d_th / 2.0f);
            odom_theta += d_th;
            vel_linear  = d_c / dt;
            vel_angular = d_th / dt;
        }
    }

    // -----------------------------------------------------------------------
    // 30 Hz — odom + IMU publish
    // -----------------------------------------------------------------------
    if (now - last_pub_ms >= 33) {
        last_pub_ms = now;

        microros_publish_odom(odom_x, odom_y, odom_theta, vel_linear, vel_angular);

        // IMU read — take I2C mutex to coordinate with battery_task on Core 0
        if (g_i2c_mutex && xSemaphoreTake(g_i2c_mutex, pdMS_TO_TICKS(10))) {
            float ax, ay, az, gx, gy, gz;
            if (imu_read(&ax, &ay, &az, &gx, &gy, &gz)) {
                xSemaphoreGive(g_i2c_mutex);
                microros_publish_imu(ax, ay, az, gx, gy, gz);
            } else {
                xSemaphoreGive(g_i2c_mutex);
            }
        }
    }

    // -----------------------------------------------------------------------
    // 1 Hz — battery publish via micro-ROS
    // -----------------------------------------------------------------------
    if (now - last_bat_pub_ms >= 1000) {
        last_bat_pub_ms = now;
        microros_publish_battery(g_battery.voltage_v, g_battery.current_ma);
    }

    // -----------------------------------------------------------------------
    // micro-ROS state machine + executor spin (every iteration)
    // -----------------------------------------------------------------------
    microros_spin();
}
