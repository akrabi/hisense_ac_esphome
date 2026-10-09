#include "test_support.h"
#include "transport.h"
#include "commands.h"
#include "command_goldens.h"
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

void check_swing_packet(const Bytes &packet, uint8_t axis, bool enabled) {
    const auto *expected = axis == 2 ? (enabled ? golden::vert_swing : golden::vert_dir) :
                                      (enabled ? golden::hor_swing : golden::hor_dir);
    CHECK(packet == Bytes(expected, expected + CMD_SIZE));
    CHECK(packet[32] == (axis == 2 ? (enabled ? 0xC0 : 0x40) : (enabled ? 0x30 : 0x10)));
    auto sealed = packet;
    seal(sealed);
    CHECK(packet == wire(sealed));
}

void swing_encoding() {
    for (uint8_t axis : {1, 2}) {
        for (bool enabled : {false, true}) {
            CommandPacket packet;
            CHECK(encode_swing_axis(axis == 2 ? SwingAxis::VERTICAL : SwingAxis::HORIZONTAL, enabled, packet));
            check_swing_packet(Bytes(packet.data, packet.data + packet.size), axis, enabled);
        }
    }
}

void swing_transitions() {
    for (uint8_t before = 0; before < 4; ++before) {
        for (uint8_t after = 0; after < 4; ++after) {
            Host host;
            transport::Engine engine(&host);
            transport::Request request;
            request.fields = transport::SWING;
            request.swing = after;
            uint32_t generation;
            CHECK(engine.enqueue(request, 0, generation));
            auto device = state();
            device.left_right = (before & 1) != 0;
            device.up_down = (before & 2) != 0;
            size_t observed = 0;
            unsigned controls = 0;
            unsigned confirmed_steps = 0;
            const uint8_t first_axis = before == 1 && after == 2 ? 1 : 2;
            uint8_t remaining = before ^ after;
            for (uint32_t now = 0; now < 3000; ++now) {
                engine.tick(now);
                while (observed < host.writes.size()) {
                    const auto &packet = host.writes[observed++];
                    if (packet[13] == 0x66) continue;
                    CHECK(controls == confirmed_steps);
                    const uint8_t axis = remaining & first_axis ? first_axis : first_axis ^ 3;
                    CHECK((remaining & axis) != 0);
                    remaining &= ~axis;
                    check_swing_packet(packet, axis, (after & axis) != 0);
                    ++controls;
                    if (packet[32] & 0x40) device.up_down = (packet[32] & 0x80) != 0;
                    if (packet[32] & 0x10) device.left_right = (packet[32] & 0x20) != 0;
                }
                if (now % 100 == 50 && engine.phase() == transport::Phase::WAIT_STATUS) {
                    engine.receive(device, now);
                    confirmed_steps = controls;
                }
            }
            const auto changed = before ^ after;
            CHECK(controls == unsigned(bool(changed & 1)) + unsigned(bool(changed & 2)));
            CHECK(device.left_right == bool(after & 1) && device.up_down == bool(after & 2));
            CHECK(remaining == 0);
            CHECK(host.results.size() == 1 && host.results[0].second == transport::Result::CONFIRMED);
        }
    }

    // Desired values are queued, not toggle counts based on an old UI state.
    Host host;
    transport::Engine engine(&host);
    transport::Request desired;
    desired.fields = transport::SWING;
    desired.swing = 2;
    uint32_t generation;
    CHECK(engine.enqueue(desired, 0, generation));
    desired.swing = 0;
    CHECK(engine.enqueue(desired, 0, generation));
    auto device = state();
    size_t observed = 0;
    unsigned controls = 0;
    for (uint32_t now = 0; now < 3000; ++now) {
        engine.tick(now);
        while (observed < host.writes.size()) {
            const auto &packet = host.writes[observed++];
            if (packet[13] == 0x66) continue;
            check_swing_packet(packet, 2, controls == 0);
            ++controls;
            device.up_down = (packet[32] & 0x80) != 0;
        }
        if (now % 100 == 50) engine.receive(device, now);
    }
    CHECK(controls == 2 && !device.up_down);
    CHECK(host.results.size() == 2 && host.results.back().second == transport::Result::CONFIRMED);

    // A physical remote changing an axis between steps invalidates the guard.
    Host remote;
    transport::Engine guarded(&remote);
    desired.swing = 3;
    CHECK(guarded.enqueue(desired, 0, generation));
    guarded.tick(0);
    guarded.receive(state(), 100);
    guarded.tick(100);
    tick(guarded, 101, 700);
    auto vertical = state();
    vertical.up_down = true;
    guarded.receive(vertical, 700);
    guarded.receive(state(), 701);
    guarded.tick(701);
    CHECK(remote.results.back().second == transport::Result::PREREQUISITE);
    controls = 0;
    for (const auto &packet : remote.writes) controls += packet[13] == 0x65;
    CHECK(controls == 1);
}

void swing_confirmation_failures() {
    for (uint8_t before = 0; before < 4; ++before) {
        for (uint8_t after = 0; after < 4; ++after) {
            if (before == after) continue;
            for (bool mismatch : {false, true}) {
                Host host;
                transport::Engine engine(&host);
                transport::Request request;
                request.fields = transport::SWING;
                request.swing = after;
                uint32_t generation;
                CHECK(engine.enqueue(request, 0, generation));
                auto device = state();
                device.left_right = (before & 1) != 0;
                device.up_down = (before & 2) != 0;
                engine.tick(0);
                engine.receive(device, 100);
                engine.tick(100);
                CHECK(host.writes.size() == 2);
                // A later request cannot overwrite in-flight packet storage or survive failure.
                request.swing = before;
                CHECK(engine.enqueue(request, 100, generation));
                auto early = device;
                early.left_right = (after & 1) != 0;
                early.up_down = (after & 2) != 0;
                engine.receive(early, 150);
                CHECK(host.results.empty() && engine.busy());
                for (uint32_t now = 151; now <= 12000; ++now) {
                    engine.tick(now);
                    if (mismatch && now % 100 == 50) engine.receive(device, now);
                }
                CHECK(!engine.busy() && engine.pending() == 0);
                CHECK(host.results.size() >= 2);
                CHECK(host.results[0].second == (mismatch ? transport::Result::EXPIRED : transport::Result::TIMEOUT));
                CHECK(host.results[1].second == transport::Result::CANCELLED);
                unsigned controls = 0;
                for (const auto &packet : host.writes) controls += packet[13] == 0x65;
                CHECK(controls == 1);
            }
        }
    }
}

void command_packet_lifetime() {
    Host first_host, second_host;
    transport::Engine first(&first_host), second(&second_host);
    transport::Request request;
    request.fields = transport::MODE | transport::TEMPERATURE | transport::SWING;
    request.mode = 1;
    request.temperature = 16;
    request.swing = 3;
    uint32_t generation;
    CHECK(first.enqueue(request, 0, generation));
    request.temperature = 27;
    request.swing = 0;
    CHECK(second.enqueue(request, 0, generation));
    first.tick(0); second.tick(0);
    auto both = state();
    both.left_right = both.up_down = true;
    first.receive(state(), 100); second.receive(both, 100);
    // Both have built a deferred temperature step, but first send the mode.
    request.fields = transport::TEMPERATURE;
    request.temperature = 30;
    CHECK(first.enqueue(request, 100, generation));
    first.tick(100); second.tick(100);
    CHECK(first_host.writes.back() == Bytes(mode_heat, mode_heat + CMD_SIZE));
    tick(first, 101, 700); tick(second, 101, 700);
    auto heat = state();
    heat.mode_status = 1;
    heat.indoor_temperature_setting = 18;
    first.receive(heat, 700);
    both.mode_status = 1;
    both.indoor_temperature_setting = 18;
    second.receive(both, 700);
    first.tick(701); second.tick(701);
    CHECK(first_host.writes.back() == Bytes(golden::temp_16_C, golden::temp_16_C + sizeof(golden::temp_16_C)));
    CHECK(second_host.writes.back() == Bytes(golden::temp_27_C, golden::temp_27_C + sizeof(golden::temp_27_C)));
    tick(first, 702, 1300); tick(second, 702, 1300);
    heat.indoor_temperature_setting = 16;
    both.indoor_temperature_setting = 27;
    first.receive(heat, 1300); second.receive(both, 1300);
    first.tick(1301); second.tick(1301);
    check_swing_packet(first_host.writes.back(), 2, true);
    check_swing_packet(second_host.writes.back(), 2, false);
    tick(first, 1302, 1900); tick(second, 1302, 1900);
    heat.up_down = true;
    both.up_down = false;
    first.receive(heat, 1900); second.receive(both, 1900);
    first.tick(1901); second.tick(1901);
    check_swing_packet(first_host.writes.back(), 1, true);
    check_swing_packet(second_host.writes.back(), 1, false);
}

void fan_status_confirmation() {
    for (uint8_t requested : {0, 2, 10, 14, 18}) {
        transport::Request request;
        request.fields = transport::FAN;
        request.fan = requested;
        for (unsigned raw = 0; raw <= 255; ++raw) {
            auto device = state();
            device.wind_status = static_cast<uint8_t>(raw);
            CHECK(transport::Engine::matches(device, request) ==
                  (raw == requested || (raw == 1 && requested == 0)));
        }
    }
    for (uint8_t raw : {1, 12, 16, 255}) {
        Host host;
        transport::Engine engine(&host);
        transport::Request invalid;
        invalid.fields = transport::FAN;
        invalid.fan = raw;
        uint32_t generation;
        CHECK(!engine.enqueue(invalid, 0, generation)); // Status alias is not a command.
    }

    for (uint8_t initial : {0, 1, 10}) {
        for (uint8_t reported : {0, 1}) {
            Host host;
            transport::Engine engine(&host);
            transport::Request request;
            request.fields = transport::FAN;
            request.fan = 0;
            uint32_t generation;
            CHECK(engine.enqueue(request, 0, generation));
            auto device = state();
            device.wind_status = initial;
            engine.tick(0);
            engine.receive(device, 100);
            engine.tick(100);
            if (initial == 10) {
                CHECK(host.writes.back() == Bytes(golden::speed_auto, golden::speed_auto + sizeof(golden::speed_auto)));
                device.wind_status = reported;
                engine.receive(device, 150); // Early feedback cannot confirm the setter.
                CHECK(host.results.empty());
                tick(engine, 101, 800);
                engine.receive(device, 800);
                CHECK(host.writes.size() == 3);
            } else {
                CHECK(host.writes.size() == 1); // Already Auto: no redundant setter.
            }
            CHECK(!engine.busy());
            CHECK(host.results.size() == 1 && host.results[0].second == transport::Result::CONFIRMED);
        }
    }
}

int main() {
    swing_encoding();
    swing_transitions();
    swing_confirmation_failures();
    command_packet_lifetime();
    fan_status_confirmation();
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
    CHECK(host.writes.size() == 2 && host.writes.back() == Bytes(golden::temp_24_C, golden::temp_24_C + sizeof(golden::temp_24_C)));
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

    // Missing swing confirmation is never retried.
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

    // Repeated unchanged status is not confirmation or proven rejection.
    // Poll only within the 10 s bound; never retransmit the display command.
    Host stale;
    transport::Engine reconciliation(&stale);
    transport::Request display;
    display.fields = transport::FIELD_DISPLAY;
    display.display = false;
    CHECK(reconciliation.enqueue(display, 0, generation));
    auto old_display = state(); old_display.back_led = true;
    for (uint32_t now = 0; now <= 10000; ++now) {
        reconciliation.tick(now);
        if (now % 100 == 0) reconciliation.receive(old_display, now);
    }
    CHECK(!reconciliation.busy());
    CHECK(stale.results.back().second == transport::Result::EXPIRED);
    controls = 0;
    for (const auto &packet : stale.writes) controls += packet[13] == 0x65;
    CHECK(controls == 1 && stale.writes.size() <= 22);

    Host transition;
    transport::Engine transition_engine(&transition);
    swing.swing = 2;
    CHECK(transition_engine.enqueue(swing, 0, generation));
    transition_engine.tick(0);
    auto horizontal = state(); horizontal.left_right = true;
    transition_engine.receive(horizontal, 100);
    transition_engine.tick(100);
    check_swing_packet(transition.writes.back(), 1, false);
    return 0;
}
