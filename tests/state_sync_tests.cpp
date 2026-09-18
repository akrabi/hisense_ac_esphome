#include "test_support.h"
#include "hisense_ac.h"
#include "commands.h"

uint32_t esphome::test_clock = 0;
using namespace esphome;
using namespace esphome::hisense_ac;

struct Rig {
    uart::UARTComponent bus;
    HisenseAC ac{&bus};
    sensor::Sensor frequency;
    HisenseACDisplaySwitch display{&ac};
    Rig(bool optimistic = false) {
        test_clock = 0;
        ac.set_optimistic(optimistic);
        ac.set_temperature_unit(CELSIUS);
        ac.set_compressor_frequency(&frequency);
        ac.set_display_switch(&display);
        ac.setup();
    }
    static Bytes report(uint8_t mode = 0x28, uint8_t target = 22, uint8_t current = 24) {
        auto frame = synthetic();
        frame[16] = 10;
        frame[18] = mode;
        frame[19] = target;
        frame[20] = current;
        frame[35] = 0x80;
        frame[37] = 0x80;
        frame[41] = 50;
        seal(frame);
        return frame;
    }
    void receive(const Bytes &frame, uint32_t now) {
        test_clock = now;
        auto bytes = wire(frame);
        bus.rx.insert(bus.rx.end(), bytes.begin(), bytes.end());
        ac.loop();
    }
    void until(uint32_t now) {
        while (test_clock != now) { ++test_clock; ac.loop(); }
    }
    void prime() { ac.loop(); receive(report(), 100); }
    void temperature(float value) {
        climate::ClimateCall call;
        call.requested_temperature = value;
        ac.control(call);
    }
    void mode(climate::ClimateMode value, float temperature = NAN) {
        climate::ClimateCall call;
        call.requested_mode = value;
        if (std::isfinite(temperature)) call.requested_temperature = temperature;
        ac.control(call);
    }
};

void default_reported_state() {
    Rig rig;
    rig.ac.update(); rig.ac.update(); rig.ac.loop();
    CHECK(rig.ac.publications.empty() && rig.frequency.publications.empty() && rig.display.publications.empty());
    auto invalid = Rig::report();
    invalid[20] ^= 1;
    rig.receive(invalid, 50);
    CHECK(rig.ac.publications.empty() && rig.frequency.publications.empty());
    rig.receive(Rig::report(), 100);
    CHECK(rig.ac.publications.size() == 1);
    CHECK(rig.ac.mode == climate::CLIMATE_MODE_COOL && rig.ac.target_temperature == 22);
    CHECK(rig.ac.current_temperature == 24 && rig.ac.action == climate::CLIMATE_ACTION_COOLING);
    CHECK(rig.frequency.publications.back() == 50 && rig.display.state);
    CHECK(!rig.ac.preset.has_value() && !rig.ac.warning);
    const auto published = rig.ac.publications.size();
    rig.mode(climate::CLIMATE_MODE_HEAT, 25);
    rig.display.control(false);
    CHECK(rig.ac.publications.size() == published && rig.display.state);
    CHECK(rig.ac.mode == climate::CLIMATE_MODE_COOL && rig.ac.target_temperature == 22);
    rig.ac.update();
    CHECK(rig.ac.publications.size() == published);
    rig.until(20000);
    CHECK(rig.ac.mode == climate::CLIMATE_MODE_COOL && rig.ac.action == climate::CLIMATE_ACTION_COOLING);
    CHECK(rig.ac.publications.size() == published); // No fabricated Off/initial zeros on timeout.
    CHECK(rig.ac.warning);
}

void optimistic_state_and_failures() {
    Rig rig(true);
    rig.prime();
    const auto measurements = rig.frequency.publications.size();
    rig.mode(climate::CLIMATE_MODE_HEAT, 25.4f);
    rig.display.control(false);
    CHECK(rig.ac.mode == climate::CLIMATE_MODE_HEAT && rig.ac.target_temperature == 25);
    CHECK(!rig.display.state);
    CHECK(rig.ac.current_temperature == 24 && rig.ac.action == climate::CLIMATE_ACTION_COOLING);
    CHECK(rig.frequency.publications.size() == measurements);
    rig.until(1000); // Silence: failed baseline cancels the entire operation and dependent queue.
    CHECK(rig.ac.mode == climate::CLIMATE_MODE_COOL && rig.ac.target_temperature == 22);
    CHECK(rig.display.state && rig.ac.warning);
    CHECK(rig.frequency.publications.size() == measurements);
    rig.until(20000);
    const auto writes = rig.bus.tx.size();
    rig.receive(Rig::report(0x18, 27, 25), 20001); // Remote heat setting on reconnection is authoritative.
    CHECK(rig.ac.mode == climate::CLIMATE_MODE_HEAT && rig.ac.target_temperature == 27);
    CHECK(rig.bus.tx.size() == writes);
    for (const auto &packet : rig.bus.tx) CHECK(packet[13] == 0x66); // No expired control replay.

    Rig unknown(true);
    unknown.mode(climate::CLIMATE_MODE_HEAT, 25);
    unknown.display.control(true);
    CHECK(unknown.ac.publications.empty() && unknown.frequency.publications.empty());
    CHECK(unknown.display.state);
    unknown.until(10001);
    CHECK(unknown.ac.publications.empty());
    CHECK(unknown.display.state); // Cannot restore a guessed Off when no report exists.
}

void generations_and_reconciliation() {
    Rig rig(true);
    rig.prime();
    rig.temperature(23);
    rig.ac.loop(); // Baseline query.
    rig.receive(Rig::report(), 200);
    CHECK(rig.bus.tx.back() == Bytes(temp_23_C, temp_23_C + CMD_SIZE));
    rig.temperature(26);
    CHECK(rig.ac.target_temperature == 26);
    rig.receive(Rig::report(0x28, 23), 250); // Setter response / old requested target.
    CHECK(rig.ac.target_temperature == 26);
    rig.until(800);
    CHECK(rig.bus.tx.back()[13] == 0x66);
    rig.receive(Rig::report(0x28, 23), 801); // Completes older generation, not newer overlay.
    CHECK(rig.ac.target_temperature == 26);
    rig.receive(Rig::report(0x28, 23), 900);
    CHECK(rig.bus.tx.back() == Bytes(temp_26_C, temp_26_C + CMD_SIZE));
    rig.receive(Rig::report(0x28, 23), 1000);
    CHECK(rig.ac.target_temperature == 26);
    rig.until(1500);
    rig.receive(Rig::report(0x28, 26), 1501);
    CHECK(rig.ac.target_temperature == 26 && !rig.ac.warning);

    PendingState pending;
    transport::Request first, newer, confirmed;
    first.fields = transport::MODE | transport::TEMPERATURE;
    first.mode = 1; first.temperature = 23;
    newer.fields = transport::TEMPERATURE; newer.temperature = 27;
    pending.accept(first, 1, UINT32_MAX - 10);
    pending.accept(newer, 2, UINT32_MAX - 5);
    CHECK(pending.complete(1));
    CHECK(pending.fields() == transport::TEMPERATURE);
    CHECK(pending.present(confirmed, true).temperature == 27);
    CHECK(pending.present(confirmed, false).fields == 0);
    CHECK(!pending.expire(100));
    CHECK(pending.expire(10000));
    CHECK(pending.fields() == 0);
}

void unknown_fields_and_presets() {
    Rig rig;
    rig.prime();
    auto unknown = Rig::report(0xF8, 255, 0);
    unknown[16] = 255;
    seal(unknown);
    rig.receive(unknown, 200);
    CHECK(rig.ac.mode == climate::CLIMATE_MODE_COOL && rig.ac.target_temperature == 22);
    CHECK(rig.ac.current_temperature == 24 && rig.ac.fan_mode == climate::CLIMATE_FAN_LOW);
    CHECK(rig.ac.action == climate::CLIMATE_ACTION_COOLING);
    climate::ClimateCall preset;
    preset.requested_preset = climate::CLIMATE_PRESET_BOOST;
    rig.ac.control(preset);
    CHECK(!rig.ac.preset.has_value());
    rig.ac.loop();
    rig.receive(Rig::report(), 300);
    CHECK(rig.bus.tx.back() == Bytes(turbo_on, turbo_on + CMD_SIZE));
    rig.until(900);
    rig.receive(Rig::report(), 901);
    CHECK(!rig.ac.preset.has_value() && rig.ac.warning);

    Rig optimistic(true);
    optimistic.prime();
    optimistic.ac.control(preset);
    CHECK(optimistic.ac.preset == climate::CLIMATE_PRESET_BOOST);
    optimistic.ac.loop();
    optimistic.receive(Rig::report(), 200);
    optimistic.until(800);
    optimistic.receive(Rig::report(), 801);
    CHECK(!optimistic.ac.preset.has_value()); // Unverified is not confirmed NONE.
}

void stable_targets_and_atomic_rejection() {
    Rig rig(true);
    rig.prime(); // Remember cool 22.
    rig.mode(climate::CLIMATE_MODE_HEAT, 27);
    rig.ac.loop();
    rig.receive(Rig::report(), 200);
    CHECK(rig.bus.tx.back() == Bytes(mode_heat, mode_heat + CMD_SIZE));
    rig.receive(Rig::report(0x18, 16), 250); // Interim mode side-effect setpoint.
    rig.until(800);
    rig.receive(Rig::report(0x18, 16), 801);
    CHECK(rig.bus.tx.back() == Bytes(temp_27_C, temp_27_C + CMD_SIZE));
    rig.until(1400);
    rig.receive(Rig::report(0x18, 27), 1401);
    rig.mode(climate::CLIMATE_MODE_COOL); // Restore stable cool 22, not interim 16.
    rig.ac.loop();
    rig.receive(Rig::report(0x18, 27), 1500);
    CHECK(rig.bus.tx.back() == Bytes(mode_cool, mode_cool + CMD_SIZE));
    rig.until(2100);
    rig.receive(Rig::report(0x28, 16), 2101);
    CHECK(rig.bus.tx.back() == Bytes(temp_22_C, temp_22_C + CMD_SIZE));

    Rig full(true);
    full.prime();
    for (unsigned i = 0; i < 8; ++i) full.mode(climate::CLIMATE_MODE_OFF);
    const auto published = full.ac.publications.size();
    const auto displayed = full.display.publications.size();
    full.mode(climate::CLIMATE_MODE_HEAT, 29);
    full.display.control(false);
    CHECK(full.ac.publications.size() == published && full.display.publications.size() == displayed);
    CHECK(full.ac.mode == climate::CLIMATE_MODE_OFF && full.ac.target_temperature == 22);
    CHECK(full.bus.tx.size() == 1); // Atomic acceptance/rejection never sends in callbacks.
}

void display_reconciliation_and_instances() {
    Rig rig(true);
    rig.prime();
    rig.display.control(false);
    CHECK(!rig.display.state);
    rig.ac.loop();
    rig.receive(Rig::report(), 200);
    CHECK(rig.bus.tx.back() == Bytes(display_off, display_off + CMD_SIZE));
    rig.until(800);
    rig.receive(Rig::report(), 801); // Device still reports the old display value.
    CHECK(!rig.display.state);
    rig.until(1400);
    rig.receive(Rig::report(), 1401);
    CHECK(!rig.display.state);
    rig.until(2000);
    auto display_off_report = Rig::report();
    display_off_report[37] = 0;
    seal(display_off_report);
    rig.receive(display_off_report, 2001);
    CHECK(!rig.display.state && !rig.ac.warning);
    unsigned writes = 0;
    for (const auto &packet : rig.bus.tx) writes += packet[13] == 0x65;
    CHECK(writes == 1); // Only queries repeat, never the command.
    rig.receive(Rig::report(), 2100);
    CHECK(rig.display.state); // Physical remote is authoritative with no overlay.

    Rig first(true), second;
    first.prime();
    second.ac.loop();
    second.receive(Rig::report(0x18, 27, 29), 200);
    CHECK(first.ac.target_temperature == 22 && second.ac.target_temperature == 27);
    first.temperature(25);
    CHECK(first.ac.target_temperature == 25 && second.ac.target_temperature == 27);
    CHECK(first.ac.current_temperature == 24 && second.ac.current_temperature == 29);
}

int main() {
    default_reported_state();
    optimistic_state_and_failures();
    generations_and_reconciliation();
    unknown_fields_and_presets();
    stable_targets_and_atomic_rejection();
    display_reconciliation_and_instances();
    return 0;
}
