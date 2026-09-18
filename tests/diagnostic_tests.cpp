#include "test_support.h"
#include "hisense_ac.h"

uint32_t esphome::test_clock = 0;
using namespace esphome;
using namespace esphome::hisense_ac;

struct Diagnostics {
    uart::UARTComponent bus;
    HisenseAC ac{&bus};
    binary_sensor::BinarySensor connected;
    sensor::Sensor age, invalid, timeouts, rejected;
    Diagnostics() {
        ac.set_communication_connected(&connected);
        ac.set_last_status_age(&age);
        ac.set_invalid_frame_count(&invalid);
        ac.set_response_timeout_count(&timeouts);
        ac.set_queue_rejection_count(&rejected);
        ac.setup();
    }
    void loop(uint32_t now) { test_clock = now; ac.loop(); }
    void receive(Bytes packet, uint32_t now) {
        bus.rx.insert(bus.rx.end(), packet.begin(), packet.end());
        loop(now);
    }
};

Bytes status() {
    auto frame = synthetic();
    frame[16] = 10;
    frame[18] = 0x28;
    frame[19] = 22;
    frame[20] = 24;
    seal(frame);
    return frame;
}

int main() {
    test_clock = 0;
    Diagnostics rig;
    CHECK(rig.connected.has_state() && !rig.connected.state);
    CHECK(!rig.age.has_state());
    CHECK(rig.invalid.get_raw_state() == 0 && rig.timeouts.get_raw_state() == 0 &&
          rig.rejected.get_raw_state() == 0);
    rig.loop(0);
    rig.receive(wire(status()), 100);
    CHECK(rig.connected.state && rig.age.get_raw_state() == 0);
    rig.loop(1200);
    CHECK(rig.age.get_raw_state() == 1);
    const auto age_publications = rig.age.publications.size();
    rig.loop(1201);
    CHECK(rig.age.publications.size() == age_publications);

    auto invalid = status();
    invalid[20] ^= 1;
    rig.receive(wire(invalid), 1300);
    CHECK(rig.invalid.get_raw_state() == 1);
    CHECK(rig.ac.target_temperature == 22);
    rig.receive({0xF4, 0xF5, 0x01, 0x40, 0x49}, 1400);
    rig.loop(1500);
    CHECK(rig.invalid.get_raw_state() == 2);

    rig.ac.update();
    rig.loop(2000);
    rig.loop(2600);
    rig.loop(2601);
    CHECK(rig.timeouts.get_raw_state() == 1 && !rig.connected.state);
    rig.receive(wire(status()), 2700);
    CHECK(rig.connected.state);
    const auto reported_mode = rig.ac.mode;
    rig.loop(30000);
    rig.loop(30001);
    CHECK(!rig.connected.state && rig.ac.mode == reported_mode);

    test_clock = 0;
    Diagnostics slow;
    slow.ac.interval_ = 60000;
    slow.loop(0);
    slow.receive(wire(status()), 100);
    slow.loop(40000);
    CHECK(slow.connected.state);
    slow.loop(180100);
    CHECK(!slow.connected.state);

    test_clock = UINT32_MAX - 100;
    Diagnostics rollover;
    rollover.loop(test_clock);
    rollover.receive(wire(status()), UINT32_MAX - 50);
    rollover.loop(1500);
    CHECK(rollover.connected.state && rollover.age.get_raw_state() == 1);

    test_clock = 0;
    Diagnostics restricted;
    restricted.ac.set_optimistic(true);
    restricted.ac.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_COOL});
    restricted.ac.set_supported_swing_modes({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL});
    restricted.ac.set_supported_presets({});
    const auto traits = restricted.ac.traits();
    CHECK(traits.supports_mode(climate::CLIMATE_MODE_COOL) && !traits.supports_mode(climate::CLIMATE_MODE_HEAT));
    CHECK(traits.supports_swing_mode(climate::CLIMATE_SWING_VERTICAL) &&
          !traits.supports_swing_mode(climate::CLIMATE_SWING_HORIZONTAL));
    CHECK(!traits.supports_preset(climate::CLIMATE_PRESET_BOOST));
    restricted.loop(0);
    restricted.receive(wire(status()), 100);
    const auto publications = restricted.ac.publications.size();
    climate::ClimateCall call;
    call.requested_mode = climate::CLIMATE_MODE_HEAT;
    restricted.ac.control(call);
    call.requested_mode.reset();
    call.requested_swing = climate::CLIMATE_SWING_HORIZONTAL;
    restricted.ac.control(call);
    call.requested_swing.reset();
    call.requested_preset = climate::CLIMATE_PRESET_BOOST;
    restricted.ac.control(call);
    CHECK(restricted.ac.publications.size() == publications);
    restricted.loop(101);
    CHECK(restricted.rejected.get_raw_state() == 0 && restricted.bus.tx.size() == 1);
    call.requested_preset.reset();
    call.requested_mode = climate::CLIMATE_MODE_OFF;
    for (unsigned i = 0; i < 9; ++i) restricted.ac.control(call);
    restricted.ac.set_display(true);
    restricted.loop(102);
    CHECK(restricted.rejected.get_raw_state() == 2);
    CHECK(rig.rejected.get_raw_state() == 0); // Independent instances/counters.
    return 0;
}
