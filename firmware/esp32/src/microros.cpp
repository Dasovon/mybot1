#include "microros.h"
#include "motors.h"

#include <micro_ros_arduino.h>

// micro-ROS transport: Serial (native USB CDC, GPIO 19/20) → Pi /dev/ttyACM0
// setTxTimeoutMs(0) prevents blocking writes before the host opens the port.
extern "C" bool arduino_transport_open(struct uxrCustomTransport * transport) {
    (void)transport;
    Serial.setTxTimeoutMs(100);  // allow buffer to drain; 0 causes partial writes during entity creation
    Serial.begin(921600);
    return true;
}

extern "C" bool arduino_transport_close(struct uxrCustomTransport * transport) {
    (void)transport;
    return true;
}

extern "C" size_t arduino_transport_write(struct uxrCustomTransport* transport,
                                           const uint8_t* buf, size_t len, uint8_t* err) {
    (void)transport; (void)err;
    // No flush — TX hardware drains at 921600 baud in background (~2ms per fragment).
    // Serial.flush() blocked for up to 100ms per fragment (×3 odom fragments), stalling
    // the 100Hz PID loop and preventing integrator accumulation.
    return Serial.write(buf, len);
}

extern "C" size_t arduino_transport_read(struct uxrCustomTransport* transport,
                                          uint8_t* buf, size_t len, int timeout_ms,
                                          uint8_t* err) {
    (void)transport; (void)err;
    // Non-blocking read (0 ms): XRCE calls transport_read in a tight loop during
    // RELIABLE ACK waits; any non-zero cap × many calls = multi-second stalls that
    // halt PID and trigger the cmd_vel watchdog. Data arriving in the USB CDC buffer
    // is still read immediately — setTimeout(0) only changes the wait-for-first-byte
    // behavior when the buffer is empty.
    Serial.setTimeout(0);
    return Serial.readBytes((char*)buf, len);
}

#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <rmw_microros/rmw_microros.h>

#include <nav_msgs/msg/odometry.h>
#include <sensor_msgs/msg/imu.h>
#include <geometry_msgs/msg/twist.h>

// ---------------------------------------------------------------------------
// Shared cmd_vel state (written by subscriber callback, read by loop())
// ---------------------------------------------------------------------------
static volatile float    g_cmd_linear  = 0.0f;
static volatile float    g_cmd_angular = 0.0f;
static volatile uint32_t g_last_cmd_ms = 0;

// ---------------------------------------------------------------------------
// micro-ROS objects
// ---------------------------------------------------------------------------
static rcl_node_t       node;
static rclc_support_t   support;
static rcl_allocator_t  allocator;
static rclc_executor_t  executor;

static rcl_publisher_t    pub_odom;
static rcl_publisher_t    pub_imu;
static rcl_subscription_t sub_cmd_vel;

static nav_msgs__msg__Odometry   odom_msg;
static sensor_msgs__msg__Imu     imu_msg;
static geometry_msgs__msg__Twist cmd_vel_msg;

// Frame ID string buffers — set once in create_entities(), reused every publish.
static char frame_odom[]      = "odom";
static char frame_base_link[] = "base_link";
static char frame_imu_link[]  = "imu_link";

// ---------------------------------------------------------------------------
// State machine
// ---------------------------------------------------------------------------
enum State { WAITING_AGENT, AGENT_CONNECTED, AGENT_DISCONNECTED };
static State state = WAITING_AGENT;
static uint32_t last_ping_ms   = 0;
static uint32_t last_pub_ok_ms = 0;

// WAITING_AGENT: ping every 1 s until agent responds.
static const uint32_t PING_INTERVAL_MS = 1000;

// AGENT_CONNECTED: ping every 2 s to detect agent loss.
// 3 consecutive failures → session dead → destroy + reconnect.
// Root bug this fixes: rcl_publish returns OK even to a dead agent (kernel
// USB buffer accepts bytes regardless), so the old publish-watchdog never
// fired. Active pinging is the only reliable detection mechanism.
static const uint32_t CONNECTED_PING_INTERVAL_MS = 2000;
static const int      PING_FAIL_MAX              = 3;
static int            ping_fail_count            = 0;

// Backup watchdog: if no odom published successfully in 10 s, session is
// dead regardless of ping state. Secondary to the ping mechanism.
static const uint32_t PUB_TIMEOUT_MS = 10000;

static bool create_entities() {
    allocator = rcl_get_default_allocator();

    if (rclc_support_init(&support, 0, nullptr, &allocator) != RCL_RET_OK) return false;
    if (rclc_node_init_default(&node, "esp32_base_node", "", &support) != RCL_RET_OK) return false;

    // Odom — RELIABLE: nav_msgs/Odometry serializes to ~712 bytes which exceeds the
    // 512-byte XRCE custom-transport MTU. RELIABLE streams support fragmentation;
    // BEST_EFFORT does not, so odom silently drops on BEST_EFFORT.
    if (rclc_publisher_init_default(
            &pub_odom, &node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(nav_msgs, msg, Odometry),
            "diff_cont/odom") != RCL_RET_OK) return false;

    // IMU — RELIABLE (consistent with odom; sensor_msgs/Imu is ~312 bytes but
    // RELIABLE avoids any future size surprises and matches reference design)
    if (rclc_publisher_init_default(
            &pub_imu, &node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, Imu),
            "imu/imu") != RCL_RET_OK) return false;

    // cmd_vel subscriber — RELIABLE
    if (rclc_subscription_init_default(
            &sub_cmd_vel, &node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist),
            "diff_cont/cmd_vel_unstamped") != RCL_RET_OK) return false;

    // Executor: 1 handle (cmd_vel subscriber)
    if (rclc_executor_init(&executor, &support.context, 1, &allocator) != RCL_RET_OK) return false;
    rclc_executor_add_subscription(
        &executor, &sub_cmd_vel, &cmd_vel_msg,
        [](const void* msg) {
            const auto* m = (const geometry_msgs__msg__Twist*)msg;
            g_cmd_linear  = (float)m->linear.x;
            g_cmd_angular = (float)m->angular.z;
            g_last_cmd_ms = millis();
        },
        ON_NEW_DATA);

    // Pre-assign frame IDs — avoids allocation on every publish call
    odom_msg.header.frame_id.data     = frame_odom;
    odom_msg.header.frame_id.size     = 4;
    odom_msg.header.frame_id.capacity = 5;
    odom_msg.child_frame_id.data      = frame_base_link;
    odom_msg.child_frame_id.size      = 9;
    odom_msg.child_frame_id.capacity  = 10;

    imu_msg.header.frame_id.data      = frame_imu_link;
    imu_msg.header.frame_id.size      = 8;
    imu_msg.header.frame_id.capacity  = 9;

    // Odometry covariance — encoder-derived, constant per-axis uncertainty.
    // Zero covariance = "perfect certainty" which is never true and causes
    // robot_localization to silently patch the values. Set realistic diagonals.
    // Unused axes (z, roll, pitch for a 2D diff-drive) set to large value.
    // Pose covariance (6×6, row-major index = row*6 + col):
    odom_msg.pose.covariance[0]  = 0.05;   // x
    odom_msg.pose.covariance[7]  = 0.05;   // y
    odom_msg.pose.covariance[14] = 1e9;    // z  (unused)
    odom_msg.pose.covariance[21] = 1e9;    // roll  (unused)
    odom_msg.pose.covariance[28] = 1e9;    // pitch (unused)
    odom_msg.pose.covariance[35] = 0.10;   // yaw — the one r_l was patching

    // Twist covariance:
    odom_msg.twist.covariance[0]  = 0.01;  // vx
    odom_msg.twist.covariance[7]  = 1e9;   // vy  (diff-drive: no lateral motion)
    odom_msg.twist.covariance[14] = 1e9;   // vz  (unused)
    odom_msg.twist.covariance[21] = 1e9;   // vroll  (unused)
    odom_msg.twist.covariance[28] = 1e9;   // vpitch (unused)
    odom_msg.twist.covariance[35] = 0.05;  // vyaw

    // Orientation not provided (magnetometer disabled) — flag with covariance[0] = -1
    imu_msg.orientation_covariance[0] = -1.0;

    return true;
}

static void destroy_entities() {
    rclc_executor_fini(&executor);
    rcl_publisher_fini(&pub_odom, &node);
    rcl_publisher_fini(&pub_imu,  &node);
    rcl_subscription_fini(&sub_cmd_vel, &node);
    rcl_node_fini(&node);
    rclc_support_fini(&support);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void microros_init() {
    // Serial (USB CDC, GPIO 19/20) opened in arduino_transport_open on first agent ping
    // Serial0 (UART0 via CH340, GPIO 43/44) used for debug output → Pi /dev/ttyUSB0
    Serial0.begin(115200);
    Serial0.printf("[uROS] init: PING_INTERVAL=%lums CONNECTED_PING=%lums FAIL_MAX=%d PUB_TIMEOUT=%lums\n",
                   (unsigned long)PING_INTERVAL_MS, (unsigned long)CONNECTED_PING_INTERVAL_MS,
                   PING_FAIL_MAX, (unsigned long)PUB_TIMEOUT_MS);
    set_microros_transports();
    state          = WAITING_AGENT;
    last_ping_ms   = millis() - PING_INTERVAL_MS;  // ping on first spin, not after a delay
    ping_fail_count = 0;
}

void microros_spin() {
    uint32_t now = millis();

    switch (state) {
        case WAITING_AGENT:
            if (now - last_ping_ms >= PING_INTERVAL_MS) {
                last_ping_ms = now;
                if (rmw_uros_ping_agent(500, 3) == RMW_RET_OK) {
                    Serial0.printf("[uROS] agent found, creating entities t=%lums\n", (unsigned long)now);
                    if (create_entities()) {
                        // Sync ESP32 clock to Pi wall clock so odom/IMU headers carry
                        // Unix epoch time, not boot uptime. Without this, Pi TF uses
                        // wall clock while ESP32 stamps use uptime (~527 s), causing
                        // TF_OLD_DATA rejections and EKF measurement drops.
                        if (rmw_uros_sync_session(1000) != RMW_RET_OK) {
                            Serial0.printf("[uROS] time sync FAILED, destroying t=%lums\n",
                                           (unsigned long)now);
                            destroy_entities();
                            // Stay in WAITING_AGENT — last_ping_ms already updated
                            break;
                        }
                        int64_t epoch_ms = rmw_uros_epoch_millis();
                        Serial0.printf("[uROS] CONNECTED + TIME_SYNCED epoch_ms=%lld local_ms=%lu\n",
                                       (long long)epoch_ms, (unsigned long)millis());
                        last_pub_ok_ms  = now;
                        last_ping_ms    = now;
                        ping_fail_count = 0;
                        state = AGENT_CONNECTED;
                    } else {
                        Serial0.printf("[uROS] create_entities FAILED, retrying t=%lums\n", (unsigned long)now);
                    }
                }
            }
            break;

        case AGENT_CONNECTED:
            rclc_executor_spin_some(&executor, RCL_MS_TO_NS(1));

            // Active liveness ping every CONNECTED_PING_INTERVAL_MS.
            // Uses a fail counter so a single transient USB glitch doesn't tear down
            // a healthy session. 3 consecutive failures = agent is truly gone.
            // This is the primary detection mechanism — rcl_publish returns OK even
            // to a dead agent (kernel USB buffer accepts bytes), so the publish
            // watchdog below is only a backup.
            if (now - last_ping_ms >= CONNECTED_PING_INTERVAL_MS) {
                last_ping_ms = now;
                if (rmw_uros_ping_agent(200, 1) != RMW_RET_OK) {
                    ping_fail_count++;
                    Serial0.printf("[uROS] ping fail %d/%d t=%lums\n",
                                   ping_fail_count, PING_FAIL_MAX, (unsigned long)now);
                    if (ping_fail_count >= PING_FAIL_MAX) {
                        Serial0.printf("[uROS] agent lost — stopping motors, destroying t=%lums\n",
                                       (unsigned long)now);
                        motors_stop();
                        destroy_entities();
                        state = AGENT_DISCONNECTED;
                    }
                } else {
                    ping_fail_count = 0;
                }
            }

            // Backup publish watchdog — catches cases where pings succeed but the
            // session is silently not delivering data (e.g. transport corruption).
            if (now - last_pub_ok_ms > PUB_TIMEOUT_MS) {
                Serial0.printf("[uROS] publish watchdog fired, stopping motors, destroying t=%lums\n",
                               (unsigned long)now);
                motors_stop();
                destroy_entities();
                ping_fail_count = 0;
                state = AGENT_DISCONNECTED;
            }
            break;

        case AGENT_DISCONNECTED:
            Serial0.printf("[uROS] DISCONNECTED → WAITING t=%lums\n", (unsigned long)now);
            ping_fail_count = 0;
            state           = WAITING_AGENT;
            last_ping_ms    = now - PING_INTERVAL_MS;  // ping on next spin, not after a full delay
            break;
    }
}

void microros_publish_odom(float x, float y, float theta,
                           float vel_linear, float vel_angular) {
    if (state != AGENT_CONNECTED) return;

    int64_t epoch_ns = rmw_uros_epoch_nanos();
    odom_msg.header.stamp.sec     = static_cast<int32_t>(epoch_ns / 1000000000LL);
    odom_msg.header.stamp.nanosec = static_cast<uint32_t>(epoch_ns % 1000000000LL);

    odom_msg.pose.pose.position.x    = x;
    odom_msg.pose.pose.position.y    = y;
    odom_msg.pose.pose.position.z    = 0.0;
    odom_msg.pose.pose.orientation.x = 0.0;
    odom_msg.pose.pose.orientation.y = 0.0;
    odom_msg.pose.pose.orientation.z = sinf(theta / 2.0f);
    odom_msg.pose.pose.orientation.w = cosf(theta / 2.0f);

    odom_msg.twist.twist.linear.x  = vel_linear;
    odom_msg.twist.twist.angular.z = vel_angular;

    if (rcl_publish(&pub_odom, &odom_msg, nullptr) == RCL_RET_OK) {
        last_pub_ok_ms = millis();
    }
}

void microros_publish_imu(float ax, float ay, float az,
                          float gx, float gy, float gz) {
    if (state != AGENT_CONNECTED) return;

    int64_t epoch_ns = rmw_uros_epoch_nanos();
    imu_msg.header.stamp.sec     = static_cast<int32_t>(epoch_ns / 1000000000LL);
    imu_msg.header.stamp.nanosec = static_cast<uint32_t>(epoch_ns % 1000000000LL);

    imu_msg.linear_acceleration.x = ax;
    imu_msg.linear_acceleration.y = ay;
    imu_msg.linear_acceleration.z = az;
    imu_msg.angular_velocity.x    = gx;
    imu_msg.angular_velocity.y    = gy;
    imu_msg.angular_velocity.z    = gz;

    rcl_publish(&pub_imu, &imu_msg, nullptr);
}

float    microros_cmd_linear()   { return g_cmd_linear; }
float    microros_cmd_angular()  { return g_cmd_angular; }
uint32_t microros_last_cmd_ms()  { return g_last_cmd_ms; }
