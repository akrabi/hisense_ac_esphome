#pragma once
#include <cstddef>
#include <cstdint>
#include "device_status.h"
#include "commands.h"

namespace esphome {
namespace hisense_ac {
namespace transport {

enum Field : uint8_t { MODE = 1, TEMPERATURE = 2, FAN = 4, SWING = 8, PRESET = 16, FIELD_DISPLAY = 32, POWER = 64 };
constexpr uint8_t MODE_OFF = 4;
constexpr size_t QUEUE_CAPACITY = 8;
constexpr size_t MAX_PACKET_SIZE = MAX_COMMAND_WIRE_SIZE;
constexpr uint32_t RESPONSE_TIMEOUT_MS = 500;
constexpr uint32_t OPERATION_TIMEOUT_MS = 10000;

struct Request {
    uint8_t fields{0};
    uint8_t mode{MODE_OFF};
    float temperature{0};  // Celsius; converted only for protocol I/O.
    uint8_t fan{0};
    uint8_t swing{0};  // horizontal=1, vertical=2
    uint8_t preset{0};
    bool display{false};
    bool fahrenheit{false};
};

enum class Result { CONFIRMED, UNVERIFIED, TIMEOUT, EXPIRED, CANCELLED, SUPERSEDED, UNSUPPORTED, PREREQUISITE, WRITE_FAILED };
enum class Phase { IDLE, CONTROL_READY, CONTROL_DRAIN, CONTROL_SETTLE, POLL_DRAIN, WAIT_STATUS, RECOVERY };

class Listener {
public:
    virtual bool send_packet(const uint8_t *data, size_t size) = 0;
    virtual void operation_finished(uint32_t generation, Result result, const Request &request) = 0;
};

class Engine {
public:
    explicit Engine(Listener *listener) : listener_(listener) {}
    Engine(const Engine &) = delete;
    Engine &operator=(const Engine &) = delete;
    bool enqueue(const Request &request, uint32_t now, uint32_t &generation);
    void request_poll();
    void tick(uint32_t now);
    void receive(const DeviceStatus &status, uint32_t now);
    bool busy() const { return active_; }
    size_t pending() const { return count_; }
    Phase phase() const { return phase_; }
    // Dedicated ESP32 FIFO, 9600 8N1: rounded wire duration plus 2 ms margin.
    // This models FIFO drain; it is not a measured callback-latency guarantee.
    static uint32_t wire_time_ms(size_t size) { return static_cast<uint32_t>((size * 10000 + 9599) / 9600 + 2); }
    static bool matches(const DeviceStatus &status, const Request &request);

private:
    struct Operation { Request request; uint32_t generation{0}; uint32_t queued_at{0}; };
    struct Step { const uint8_t *data{nullptr}; size_t size{0}; Request expected; Request guard; };
    static constexpr size_t MAX_STEPS = 10;
    Listener *listener_;
    Operation queue_[QUEUE_CAPACITY]{};
    size_t count_{0};
    Operation current_{};
    Step steps_[MAX_STEPS]{};
    // One temperature step per logical operation. Never rebuild this packet
    // while that operation is in flight; queued intents contain values only.
    CommandPacket temperature_packet_{};
    size_t step_count_{0};
    size_t step_{0};
    DeviceStatus status_{};
    bool active_{false};
    bool poll_requested_{false};
    bool baseline_poll_{false};
    bool unverified_{false};
    bool recovering_{false};
    bool response_seen_{false};
    uint8_t confirmation_polls_{0};
    uint32_t next_generation_{0};
    uint32_t deadline_{0};
    uint32_t tx_until_{0};
    bool tx_started_{false};
    Phase phase_{Phase::IDLE};

    bool build_steps_();
    bool add_step_(const uint8_t *data, size_t size, Request expected, Request guard = {});
    void poll_(uint32_t now);
    bool send_(const uint8_t *data, size_t size, uint32_t now);
    void finish_(Result result, uint32_t now);
    void cancel_pending_(Result result);
    static bool due_(uint32_t now, uint32_t deadline) { return static_cast<int32_t>(now - deadline) >= 0; }
};

}  // namespace transport
}  // namespace hisense_ac
}  // namespace esphome
