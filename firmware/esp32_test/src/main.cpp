// esp32_test — bare-bones motor + encoder validation firmware.
// No micro-ROS, no PID, no EMA filter.
//
// Serial protocol (115200 baud, native USB CDC → /dev/ttyACM0):
//   RX: D,<left>,<right>\n    duty integers -255..255
//   TX: E,<vel_l>,<vel_r>,<pos_l>,<pos_r>\n  at 50 Hz (rad/s, rad)
//
// Watchdog: motors stop if no D command received within 500 ms.

#include <Arduino.h>
#include <ESP32Encoder.h>

// ── Motor pins (TB6612FNG) ──────────────────────────────────────────────────
#define MOTOR_R_CH    0
#define MOTOR_L_CH    1
#define MOTOR_R_PWM   10
#define MOTOR_R_IN1   11
#define MOTOR_R_IN2   12
#define MOTOR_L_PWM   13
#define MOTOR_L_IN1   14
#define MOTOR_L_IN2   15

// 1 kHz matches main firmware — 20 kHz caused a 10× measured speed loss on this chassis.
#define MOTOR_PWM_FREQ  1000
#define MOTOR_PWM_BITS  8

// ── Encoder pins ───────────────────────────────────────────────────────────
#define ENC_L_A  40
#define ENC_L_B  41
#define ENC_R_A  42
#define ENC_R_B  39

static constexpr float ENC_CPR  = 1010.0f;
static constexpr float DT_MS    = 20.0f;    // 50 Hz report
static constexpr float DT_S     = DT_MS / 1000.0f;
static constexpr unsigned long WATCHDOG_MS = 500;

static ESP32Encoder enc_left;
static ESP32Encoder enc_right;
static unsigned long last_cmd_ms = 0;

static void set_motor(uint8_t ch, uint8_t in1, uint8_t in2, int duty) {
    duty = constrain(duty, -255, 255);
    if (duty > 0) {
        digitalWrite(in1, HIGH); digitalWrite(in2, LOW);
    } else if (duty < 0) {
        digitalWrite(in1, LOW);  digitalWrite(in2, HIGH);
        duty = -duty;
    } else {
        digitalWrite(in1, LOW);  digitalWrite(in2, LOW);
    }
    ledcWrite(ch, (uint32_t)duty);
}

static void motors_stop() {
    set_motor(MOTOR_R_CH, MOTOR_R_IN1, MOTOR_R_IN2, 0);
    set_motor(MOTOR_L_CH, MOTOR_L_IN1, MOTOR_L_IN2, 0);
}

void setup() {
    Serial.begin(115200);

    pinMode(MOTOR_R_IN1, OUTPUT); digitalWrite(MOTOR_R_IN1, LOW);
    pinMode(MOTOR_R_IN2, OUTPUT); digitalWrite(MOTOR_R_IN2, LOW);
    pinMode(MOTOR_L_IN1, OUTPUT); digitalWrite(MOTOR_L_IN1, LOW);
    pinMode(MOTOR_L_IN2, OUTPUT); digitalWrite(MOTOR_L_IN2, LOW);

    ledcSetup(MOTOR_R_CH, MOTOR_PWM_FREQ, MOTOR_PWM_BITS);
    ledcAttachPin(MOTOR_R_PWM, MOTOR_R_CH);
    ledcSetup(MOTOR_L_CH, MOTOR_PWM_FREQ, MOTOR_PWM_BITS);
    ledcAttachPin(MOTOR_L_PWM, MOTOR_L_CH);
    motors_stop();

    // PCNT glitch filter: validated hardware path — rejects PWM switching spikes on GPIO 40/41
    ESP32Encoder::useInternalWeakPullResistors = UP;
    enc_left.attachHalfQuad(ENC_L_A, ENC_L_B);
    enc_left.setFilter(400);
    enc_right.attachHalfQuad(ENC_R_A, ENC_R_B);
    enc_right.setFilter(400);
    enc_left.clearCount();
    enc_right.clearCount();

    last_cmd_ms = millis();
    Serial.println("READY");
}

void loop() {
    static unsigned long last_report_ms = 0;
    static long prev_l = 0, prev_r = 0;

    // ── Parse incoming D commands ───────────────────────────────────────
    while (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        line.trim();
        if (line.startsWith("D,")) {
            int comma = line.indexOf(',', 2);
            if (comma > 0) {
                int dl = constrain(line.substring(2, comma).toInt(), -255, 255);
                int dr = constrain(line.substring(comma + 1).toInt(), -255, 255);
                set_motor(MOTOR_L_CH, MOTOR_L_IN1, MOTOR_L_IN2, dl);
                set_motor(MOTOR_R_CH, MOTOR_R_IN1, MOTOR_R_IN2, dr);
                last_cmd_ms = millis();
            }
        }
    }

    // ── Watchdog ────────────────────────────────────────────────────────
    if (millis() - last_cmd_ms > WATCHDOG_MS) {
        motors_stop();
    }

    // ── Encoder report at 50 Hz ─────────────────────────────────────────
    unsigned long now = millis();
    if (now - last_report_ms >= (unsigned long)DT_MS) {
        last_report_ms = now;

        long l = enc_left.getCount();
        long r = enc_right.getCount();
        float vel_l =  (float)(l - prev_l) / ENC_CPR * TWO_PI / DT_S;
        float vel_r = -(float)(r - prev_r) / ENC_CPR * TWO_PI / DT_S;  // right motor mounted mirrored
        float pos_l =  (float)l / ENC_CPR * TWO_PI;
        float pos_r = -(float)r / ENC_CPR * TWO_PI;
        prev_l = l;
        prev_r = r;

        Serial.printf("E,%.3f,%.3f,%.3f,%.3f\n", vel_l, vel_r, pos_l, pos_r);
    }
}
