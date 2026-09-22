#include "test_support.h"
#include "hisense_ac.h"
#include <sstream>

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

bool has_log(const char *tag, const std::string &text) {
    for (const auto &entry : test_logs) {
        if (entry.tag == tag && entry.message.find(text) != std::string::npos) return true;
    }
    return false;
}

Bytes logged_packet(const char *direction) {
    Bytes result;
    unsigned expected_size = 0;
    for (const auto &entry : test_logs) {
        if (entry.tag != "hisense_ac.protocol" ||
            entry.message.find(direction) == std::string::npos ||
            entry.message.find(" offset=") == std::string::npos)
            continue;
        CHECK(entry.message.find("AC=") == 0 && entry.message.size() < 256);
        const auto start = entry.message.find("bytes=");
        CHECK(start != std::string::npos);
        unsigned size, offset;
        int consumed = 0;
        CHECK(std::sscanf(entry.message.c_str() + start, "bytes=%u offset=%u: %n", &size, &offset, &consumed) == 2);
        CHECK(consumed > 0 && offset == result.size());
        if (result.empty()) expected_size = size;
        CHECK(size == expected_size);
        std::istringstream bytes(entry.message.substr(start + consumed));
        unsigned byte;
        while (bytes >> std::hex >> byte) {
            CHECK(byte <= 255);
            result.push_back(static_cast<uint8_t>(byte));
        }
        CHECK(bytes.eof() && result.size() > offset && result.size() - offset <= 32);
    }
    CHECK(expected_size != 0 && result.size() == expected_size);
    return result;
}

void packet_traces() {
    test_clock = 0;
    Diagnostics rig;
    test_logs.clear();
    rig.loop(0);
    CHECK(logged_packet("TX wire") == rig.bus.tx.back());

    test_logs.clear();
    auto frame = status();
    frame[26] = 0xF4;
    seal(frame);
    rig.receive(wire(frame), 100);
    CHECK(logged_packet("RX decoded") == frame);
    CHECK(has_log("hisense_ac.protocol", "RX decoded bytes=82 class=0x66"));
    CHECK(rig.ac.target_temperature == 22);

    // Unsupported classes and short/maximum-length frames are traced, not published.
    const auto publications = rig.ac.publications.size();
    for (const size_t size : {size_t{9}, size_t{17}, size_t{18}, size_t{82}, size_t{128}}) {
        auto unknown = synthetic(size);
        if (size >= 18) unknown[13] = 0x65;
        seal(unknown);
        test_logs.clear();
        rig.receive(wire(unknown), 200);
        CHECK(logged_packet("RX decoded") == unknown);
        CHECK(has_log("hisense_ac.protocol", size >= 18 ? "class=0x65" : "class=n/a"));
        CHECK(rig.ac.publications.size() == publications && rig.ac.target_temperature == 22);
        CHECK(rig.invalid.get_raw_state() == 0);
    }

    test_logs.clear();
    frame[20] ^= 1;
    rig.receive(wire(frame), 300);
    CHECK(!has_log("hisense_ac.protocol", "RX decoded"));
    CHECK(rig.invalid.get_raw_state() == 1);
}

void operation_traces() {
    test_clock = 0;
    Diagnostics rig;
    rig.loop(0);
    rig.receive(wire(status()), 100);
    test_logs.clear();
    climate::ClimateCall call;
    call.requested_temperature = 16.4f;
    rig.ac.control(call);
    CHECK(has_log("hisense_ac", "mode=-1 target_present=1 target=16.40C fan=-1 swing=-1 preset=-1"));
    CHECK(has_log("hisense_ac", "Operation 1 ACCEPTED: fields=0x02 target=16.00C protocol=C"));
    rig.loop(101);
    test_logs.clear();
    rig.receive(wire(status()), 200);
    CommandPacket expected;
    CHECK(encode_temperature(16, false, expected));
    CHECK(logged_packet("TX wire") == Bytes(expected.data, expected.data + expected.size));
    CHECK(rig.bus.tx.back() == Bytes(expected.data, expected.data + expected.size));
    CHECK(expected.size == 51); // Includes a stuffed checksum byte.
    rig.loop(300);
    rig.loop(800);
    auto changed = status();
    changed[19] = 16;
    seal(changed);
    rig.receive(wire(changed), 900);
    CHECK(has_log("hisense_ac", "Operation 1 CONFIRMED: fields=0x02 target=16.00C protocol=C"));
    CHECK(rig.ac.target_temperature == 16 && !rig.ac.warning);

    test_logs.clear();
    call.requested_temperature = 25.0f;
    rig.ac.control(call);
    rig.loop(901);
    rig.receive(wire(changed), 1000);
    rig.loop(1100);
    rig.loop(1700);
    rig.receive(wire(changed), 1800);
    rig.loop(10900);
    CHECK(has_log("hisense_ac", "Operation 2 EXPIRED: fields=0x02 target=25.00C protocol=C"));
    CHECK(rig.ac.target_temperature == 16 && rig.ac.warning);

    test_logs.clear();
    call.requested_temperature = 0.0f;
    call.requested_mode = climate::CLIMATE_MODE_DRY;
    call.requested_fan = climate::CLIMATE_FAN_LOW;
    call.requested_swing = climate::CLIMATE_SWING_VERTICAL;
    call.requested_preset = climate::CLIMATE_PRESET_ECO;
    rig.ac.control(call);
    const auto raw = "mode=" + std::to_string(static_cast<int>(*call.requested_mode)) +
                     " target_present=1 target=0.00C fan=" + std::to_string(static_cast<int>(*call.requested_fan)) +
                     " swing=" + std::to_string(static_cast<int>(*call.requested_swing)) +
                     " preset=" + std::to_string(static_cast<int>(*call.requested_preset));
    CHECK(has_log("hisense_ac", raw));
    CHECK(!has_log("hisense_ac", "ACCEPTED"));

    test_logs.clear();
    call.requested_temperature = 22.0f;
    rig.ac.control(call);
    CHECK(has_log("hisense_ac",
                  "Operation 3 ACCEPTED: fields=0x1F mode=3 target=22.00C fan=10 swing=2 preset=2 protocol=C"));
    rig.ac.set_display(false);
    CHECK(has_log("hisense_ac", "Operation 4 ACCEPTED: fields=0x20 display=0 protocol=C"));

    test_logs.clear();
    climate::ClimateCall off;
    off.requested_mode = climate::CLIMATE_MODE_OFF;
    off.requested_fan = climate::CLIMATE_FAN_AUTO;
    off.requested_swing = climate::CLIMATE_SWING_OFF;
    off.requested_preset = climate::CLIMATE_PRESET_NONE;
    rig.ac.control(off);
    CHECK(has_log("hisense_ac",
                  "Operation 5 ACCEPTED: fields=0x1D mode=4 fan=0 swing=0 preset=0 protocol=C"));

    test_clock = 0;
    Diagnostics silent;
    test_logs.clear();
    silent.loop(0);
    silent.loop(600);
    silent.loop(601);
    CHECK(has_log("hisense_ac", "Operation 0 TIMEOUT: fields=0x00 protocol=C"));
}

int main() {
    packet_traces();
    operation_traces();
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
