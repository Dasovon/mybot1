#include <Arduino.h>
#include "motors.h"
#include "encoders.h"
#include "pid.h"
#include "imu.h"
#include "microros.h"

// ---------------------------------------------------------------------------
// Robot physical constants (validated hardware values — do not change)
// ---------------------------------------------------------------------------
static constexpr float WHEEL_SEP   = 0.177f;   // m, center-to-center
static constexpr float WHEEL_RAD   = 0.03414f; // m (measured: 68.27mm dia)
static constexpr float ENC_CPR_F   = 990.0f;   // counts per wheel revolution (11 PPR × 2 edges × 45:1 gear, half-quad PCNT)
static constexpr float TWO_PI_F    = 2.0f * (float)M_PI;

// ---------------------------------------------------------------------------
// Watchdog — motors stop if no cmd_vel received within this window
// ---------------------------------------------------------------------------
static constexpr uint32_t WATCHDOG_MS = 500;  // pending flash + stop-time validation (≤0.6 s criterion)

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
static long  prev_ticks_r = 0;
static long  prev_ticks_l = 0;
static float vel_r_filt   = 0.0f;  // EMA-filtered wheel velocity (rad/s)
static float vel_l_filt   = 0.0f;
static constexpr float VEL_ALPHA = 1.0f;  // No EMA lag — PCNT setFilter(400) handles EMI. Reintroduce only if raw velocity is too noisy.

// ---------------------------------------------------------------------------
// Loop timing
// ---------------------------------------------------------------------------
static uint32_t last_control_ms  = 0;  // 100 Hz
static uint32_t last_pub_ms      = 0;  // 30 Hz  (odom + IMU)
static uint32_t last_enc_log_ms  = 0;  // 2 Hz   (raw PCNT count debug)

// ---------------------------------------------------------------------------
// setup
// ---------------------------------------------------------------------------
void setup() {
    Serial0.begin(115200);  // CH340 debug — must be first so encoders_init() can log
    encoders_init();
    motors_init();
    imu_init();
    microros_init();

    uint32_t now = millis();
    last_control_ms = now;
    last_pub_ms     = now;
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
        float dt_ms = dt * 1000.0f;
        last_control_ms = now;
        if (dt_ms > 15.0f) {
            Serial0.printf("[WARN] control dt %.2f ms\n", dt_ms);
        }

        if (now - microros_last_cmd_ms() > WATCHDOG_MS) {
            // No command received in time — safe stop and reset integrators
            motors_stop();
            pid_right.reset();
            pid_left.reset();
            vel_r_filt  = 0.0f;
            vel_l_filt  = 0.0f;
            vel_linear  = 0.0f;
            vel_angular = 0.0f;
        } else {
            long ticks_r = encoders_get_right();
            long ticks_l = encoders_get_left();
            long delta_r = ticks_r - prev_ticks_r;
            long delta_l = ticks_l - prev_ticks_l;
            prev_ticks_r = ticks_r;
            prev_ticks_l = ticks_l;

            // Measured wheel velocity (rad/s). Right encoder negated — motor mounted mirrored.
            float raw_r = -(float)delta_r / ENC_CPR_F * TWO_PI_F / dt;
            float raw_l =  (float)delta_l / ENC_CPR_F * TWO_PI_F / dt;
            vel_r_filt  = VEL_ALPHA * raw_r + (1.0f - VEL_ALPHA) * vel_r_filt;
            vel_l_filt  = VEL_ALPHA * raw_l + (1.0f - VEL_ALPHA) * vel_l_filt;

            // Target from latest cmd_vel
            float cmd_v = microros_cmd_linear();
            float cmd_w = microros_cmd_angular();
            float tgt_r = (cmd_v + cmd_w * WHEEL_SEP / 2.0f) / WHEEL_RAD;
            float tgt_l = (cmd_v - cmd_w * WHEEL_SEP / 2.0f) / WHEEL_RAD;

            // Feedforward + PID: apply a static friction offset the moment target is
            // nonzero so the motor overcomes stiction immediately, then let PID trim
            // the residual error. Duty is clamped to [-1, 1] inside motors_set_duty.
            float out_r = pid_right.compute(tgt_r, vel_r_filt);
            float out_l = pid_left.compute(tgt_l, vel_l_filt);
            auto apply_drive = [](float out, float tgt) -> float {
                if (fabsf(tgt) < 0.01f) return 0.0f;
                float ff   = (tgt > 0.0f) ? MOTOR_FEEDFORWARD : -MOTOR_FEEDFORWARD;
                return ff + out / MOTOR_MAX_RAD_S;
            };
            motors_set_duty(apply_drive(out_r, tgt_r), apply_drive(out_l, tgt_l));

            // Odometry integration (mid-point rule)
            float d_r  = -(float)delta_r / ENC_CPR_F * TWO_PI_F * WHEEL_RAD;  // right motor mounted mirrored
            float d_l  =  (float)delta_l / ENC_CPR_F * TWO_PI_F * WHEEL_RAD;
            float d_c  = (d_r + d_l) / 2.0f;
            float d_th = (d_r - d_l) / WHEEL_SEP;
            odom_x     += d_c * cosf(odom_theta + d_th / 2.0f);
            odom_y     += d_c * sinf(odom_theta + d_th / 2.0f);
            odom_theta += d_th;
            vel_linear  = (vel_r_filt + vel_l_filt) * 0.5f * WHEEL_RAD;
            vel_angular = (vel_r_filt - vel_l_filt) * WHEEL_RAD / WHEEL_SEP;
        }
    }

    // -----------------------------------------------------------------------
    // 30 Hz — odom + IMU publish
    // -----------------------------------------------------------------------
    if (now - last_pub_ms >= 33) {
        last_pub_ms = now;

        microros_publish_odom(odom_x, odom_y, odom_theta, vel_linear, vel_angular);

        float ax, ay, az, gx, gy, gz;
        if (imu_read(&ax, &ay, &az, &gx, &gy, &gz)) {
            microros_publish_imu(ax, ay, az, gx, gy, gz);
        }
    }

    // -----------------------------------------------------------------------
    // 2 Hz — raw PCNT count debug on Serial0 (/dev/ttyUSB0)
    // -----------------------------------------------------------------------
    if (now - last_enc_log_ms >= 500) {
        last_enc_log_ms = now;
        long raw_l = encoders_get_left();
        long raw_r = encoders_get_right();
        Serial0.printf("[ENC] L=%ld R=%ld  vel_l=%.3f vel_r=%.3f  odom_x=%.4f\n",
                       raw_l, raw_r, vel_l_filt, vel_r_filt, odom_x);
    }

    // -----------------------------------------------------------------------
    // micro-ROS state machine + executor spin (every iteration)
    // -----------------------------------------------------------------------
    microros_spin();
}
