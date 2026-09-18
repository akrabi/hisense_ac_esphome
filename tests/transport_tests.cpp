#include "test_support.h"
#include "transport.h"
#include "commands.h"
#include <utility>
using namespace esphome::hisense_ac;

struct Host : transport::Listener {
    std::vector<Bytes> writes;
    std::vector<std::pair<uint32_t, transport::Result>> results;
    bool send_packet(const uint8_t *data, size_t size) override {
        CHECK(size <= 64);
        writes.emplace_back(data, data + size);
        return true;
    }
    void operation_finished(uint32_t generation, transport::Result result, const transport::Request &) override {
        results.emplace_back(generation, result);
    }
};
DeviceStatus state() {
    DeviceStatus s;
    s.run_status = 2; s.mode_status = 2; s.indoor_temperature_setting = 21;
    return s;
}
void tick(transport::Engine &engine, uint32_t from, uint32_t to) {
    for (uint32_t now = from; now != to; ++now) engine.tick(now);
}

int main() {
    transport::Request temp;
    temp.fields = transport::TEMPERATURE; temp.temperature = 23;
    uint32_t generation;
    Host host;
    transport::Engine engine(&host);
    CHECK(engine.enqueue(temp, 0, generation));
    const auto first = generation;
    temp.temperature = 24;
    CHECK(engine.enqueue(temp, 1, generation));
    CHECK(engine.pending() == 1 && host.results.back().first == first);
    transport::Request barrier;
    barrier.fields = transport::MODE; barrier.mode = 2;
    CHECK(engine.enqueue(barrier, 2, generation));
    CHECK(engine.enqueue(temp, 3, generation));
    CHECK(engine.pending() == 3);  // Cannot coalesce across the mode barrier.
    for (unsigned i = 0; i < 5; ++i) CHECK(engine.enqueue(barrier, 4, generation));
    CHECK(!engine.enqueue(barrier, 5, generation));
    engine.tick(5);
    CHECK(engine.busy() && engine.pending() == 7 && host.writes.size() == 1);
    CHECK(engine.enqueue(barrier, 6, generation));  // Eight pending plus in-flight.
    engine.receive(state(), 10); // Poll still draining; cannot advance.
    CHECK(host.writes.size() == 1);
    engine.receive(state(), 100);
    engine.tick(100);
    CHECK(host.writes.size() == 2 && host.writes.back() == Bytes(temp_24_C, temp_24_C + CMD_SIZE));
    auto matching = state(); matching.indoor_temperature_setting = 24;
    engine.receive(matching, 120); // Stale/control response is not reconciliation.
    tick(engine, 101, 652);
    CHECK(host.writes.size() == 2);
    tick(engine, 652, 656);
    CHECK(host.writes.size() == 3 && host.writes.back()[13] == 0x66);
    engine.request_poll(); engine.request_poll(); // Dedup in-flight polls.
    engine.receive(matching, 800);
    CHECK(!engine.busy() && host.results.back().second == transport::Result::CONFIRMED);

    // Failed power/mode prerequisites cancel temperature and queued dependents.
    Host failed;
    transport::Engine failure(&failed);
    transport::Request combined = temp;
    combined.fields |= transport::MODE; combined.mode = 1;
    CHECK(failure.enqueue(combined, 0, generation));
    CHECK(failure.enqueue(temp, 0, generation));
    failure.tick(0);
    failure.receive(state(), 100);
    failure.tick(100);
    CHECK(failed.writes.back() == Bytes(mode_heat, mode_heat + CMD_SIZE));
    tick(failure, 101, 1200); // No status after post-command poll, timeout with no RX.
    CHECK(!failure.busy() && failure.pending() == 0);
    CHECK(failed.results[0].second == transport::Result::TIMEOUT);
    CHECK(failed.results[1].second == transport::Result::CANCELLED);
    const auto count = failed.writes.size();
    tick(failure, 1200, 4000); // At most one recovery poll; no control retries.
    CHECK(failed.writes.size() == count + 1);
    for (const auto &packet : failed.writes)
        CHECK(packet[13] == 0x66 || packet == Bytes(mode_heat, mode_heat + CMD_SIZE));
    failure.receive(matching, 20000);
    failure.tick(20000);
    CHECK(failed.writes.size() == count + 1); // No replay on reconnect.

    // Queue expiry is independent of RX and wrap-safe.
    Host wrapped;
    transport::Engine rollover(&wrapped);
    const uint32_t start = UINT32_MAX - 50;
    CHECK(rollover.enqueue(temp, start, generation));
    rollover.tick(start);
    rollover.receive(state(), start + 40);
    rollover.tick(start + 40);
    rollover.tick(start + 10001);
    CHECK(!rollover.busy());
    CHECK(wrapped.results.back().second == transport::Result::EXPIRED);

    // Pending polls cannot be starved by control bursts (each step reconciles).
    Host polling;
    transport::Engine poller(&polling);
    poller.request_poll(); poller.request_poll();
    CHECK(poller.enqueue(temp, 0, generation));
    poller.tick(0);
    CHECK(polling.writes.size() == 1);
    poller.receive(state(), 100);
    poller.tick(100);
    CHECK(polling.writes.size() == 2 && polling.writes.back()[13] == 0x66);

    // Toggle ambiguity is never retried.
    Host toggle;
    transport::Engine toggler(&toggle);
    transport::Request swing;
    swing.fields = transport::SWING; swing.swing = 3;
    CHECK(toggler.enqueue(swing, 0, generation));
    toggler.tick(0); toggler.receive(state(), 100); toggler.tick(100);
    tick(toggler, 101, 4000);
    unsigned controls = 0;
    for (const auto &packet : toggle.writes) controls += packet[13] == 0x65;
    CHECK(controls == 1);
    return 0;
}
