#include "test_support.h"
#include "hisense_ac.h"
#include <algorithm>
#include <climits>
#include <sstream>

uint32_t esphome::test_clock = 0;
using namespace esphome;
using namespace esphome::hisense_ac;

Bytes dry_status(uint8_t raw = 0x01, uint8_t mode_run = 0x38) {
    auto frame = synthetic();
    frame[16] = 1;  // Unmapped fan codes must not block valid Dry readback.
    frame[18] = mode_run;
    frame[19] = 27;
    frame[20] = 29;
    frame[26] = raw;
    seal(frame);
    return frame;
}

constexpr uint8_t OFFSET_READBACK[] = {
    0xF1, 0xE1, 0xD1, 0xC1, 0xB1, 0xA1, 0x91, 0x01,
    0x11, 0x21, 0x31, 0x41, 0x51, 0x61, 0x71
};

Bytes candidate(int offset) {
    CHECK(offset >= -7 && offset <= 7);
    const char *values[] = {
        "FC", "EC", "DC", "CC", "BC", "AC", "9C", "0C",
        "1C", "2C", "3C", "4C", "5C", "6C", "7C"
    };
    const char *checksums[] = {
        "02 CB", "02 BB", "02 AB", "02 9B", "02 8B", "02 7B", "02 6B", "01 DB",
        "01 EB", "01 FB", "02 0B", "02 1B", "02 2B", "02 3B", "02 4B"
    };
    const std::string hex =
        "F4 F5 00 40 29 00 00 01 01 FE 01 00 00 65 00 00 "
        "00 00 00 00 00 00 00 " + std::string(values[offset + 7]) +
        " 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 "
        "00 00 00 00 00 00 " + std::string(checksums[offset + 7]) + " F4 FB";
    Bytes result;
    std::istringstream input(hex);
    unsigned byte;
    while (input >> std::hex >> byte) result.push_back(static_cast<uint8_t>(byte));
    CHECK(result.size() == 50);
    return result;
}

bool logged(const std::string &text) {
    for (const auto &entry : test_logs)
        if (entry.tag == "hisense_ac" && entry.message.find(text) != std::string::npos) return true;
    return false;
}

struct Rig {
    uart::UARTComponent bus;
    HisenseAC ac{&bus};
    HisenseACDryOffsetNumber adjustment{&ac};
    Rig(bool enabled = true) {
        test_clock = 0;
        test_logs.clear();
        if (enabled) ac.set_dry_offset_number(&adjustment);
        ac.setup();
        loop(0);
    }
    void loop(uint32_t now) { test_clock = now; ac.loop(); }
    void receive(const Bytes &frame, uint32_t now) {
        const auto bytes = wire(frame);
        bus.rx.insert(bus.rx.end(), bytes.begin(), bytes.end());
        loop(now);
    }
    size_t setters() const {
        return std::count_if(bus.tx.begin(), bus.tx.end(), [](const Bytes &b) { return b[13] == 0x65; });
    }
    void set(float value) { static_cast<number::Number &>(adjustment).control(value); }
};

void encoding() {
    for (int offset = -7; offset <= 7; ++offset) {
        CommandPacket packet;
        CHECK(encode_dry_offset(offset, packet));
        CHECK(Bytes(packet.data, packet.data + packet.size) == candidate(offset));
        CHECK(packet.data[18] == 0 && packet.data[16] == 0 && packet.data[19] == 0);
        unsigned checksum = 0;
        for (size_t i = 2; i < packet.size - 4; ++i) checksum += packet.data[i];
        CHECK(checksum == (packet.data[46] << 8 | packet.data[47]));
    }
    for (int offset = -256; offset <= 257; ++offset) {
        if (offset >= -7 && offset <= 7) continue;
        CommandPacket packet;
        packet.size = 12;
        CHECK(!encode_dry_offset(offset, packet) && packet.size == 0);
    }
    for (int offset : {INT_MIN, INT_MAX}) {
        CommandPacket packet;
        CHECK(!encode_dry_offset(offset, packet));
    }
}

void success_and_restore(int offset) {
    const uint8_t raw = OFFSET_READBACK[offset + 7];
    Rig rig;
    rig.ac.set_optimistic(true);
    rig.receive(dry_status(), 100);
    const auto publications = rig.ac.publications.size();
    CHECK(rig.ac.set_dry_offset(offset));
    CHECK(rig.ac.publications.size() == publications && std::isnan(rig.ac.target_temperature));
    rig.loop(101);
    CHECK(rig.setters() == 0);  // Must receive the fresh baseline poll first.
    rig.receive(dry_status(), 200);
    CHECK(rig.setters() == 1 && rig.bus.tx.back() == candidate(offset));
    auto ack = dry_status(raw);
    ack[13] = 0x65;
    seal(ack);
    rig.receive(ack, 300);
    CHECK(!logged("Operation 1 CONFIRMED"));
    rig.loop(800);
    // The opposite sign and unknown negative-zero nibble must not confirm.
    rig.receive(dry_status(OFFSET_READBACK[-offset + 7]), 850);
    CHECK(!logged("Operation 1 CONFIRMED"));
    rig.receive(dry_status(0x81), 875);
    CHECK(!logged("Operation 1 CONFIRMED"));
    rig.receive(dry_status(raw), 900);
    CHECK(logged("Operation 1 CONFIRMED: fields=0x80 dry_offset=" + std::to_string(offset)));
    CHECK(rig.setters() == 1 && std::isnan(rig.ac.target_temperature) && !rig.ac.warning);
    CHECK(rig.ac.mode == climate::CLIMATE_MODE_DRY);
    CHECK(rig.adjustment.state == offset);

    CHECK(rig.ac.set_dry_offset(0));
    rig.loop(901);
    rig.receive(dry_status(raw), 1000);
    CHECK(rig.setters() == 2 && rig.bus.tx.back() == candidate(0));
    rig.loop(1100);
    rig.loop(1600);
    rig.receive(dry_status(), 1700);
    CHECK(logged("Operation 2 CONFIRMED: fields=0x80 dry_offset=0"));
    CHECK(std::isnan(rig.ac.target_temperature));
    CHECK(rig.adjustment.state == 0);

    // Already neutral: report matching readback without claiming a setter was sent.
    CHECK(rig.ac.set_dry_offset(0));
    rig.loop(1701);
    rig.receive(dry_status(), 1800);
    CHECK(logged("Operation 3 CONFIRMED"));
    CHECK(rig.setters() == 2);
}

void rejected_requests() {
    Rig rig;
    CHECK(!rig.ac.set_dry_offset(1));  // No status.
    rig.receive(dry_status(), 100);
    rig.ac.set_dry_offset_number(nullptr);
    CHECK(!rig.ac.set_dry_offset(1));
    rig.ac.set_dry_offset_number(&rig.adjustment);
    CHECK(!rig.ac.set_dry_offset(8));
    CHECK(!rig.ac.set_dry_offset(255));
    CHECK(!rig.ac.set_dry_offset(-8));
    CHECK(!rig.ac.set_dry_offset(256));
    rig.ac.set_temperature_unit(FAHRENHEIT);
    CHECK(!rig.ac.set_dry_offset(1));
    rig.ac.set_temperature_unit(CELSIUS);
    rig.ac.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_COOL});
    CHECK(!rig.ac.set_dry_offset(1));
    CHECK(rig.setters() == 0);

    for (uint8_t mode : {0x30, 0x28, 0x18, 0x08}) {
        Rig other;
        other.receive(dry_status(0x01, mode), 100);
        CHECK(!other.ac.set_dry_offset(1));
        CHECK(other.setters() == 0);
    }
    Rig queued;
    queued.receive(dry_status(), 100);
    climate::ClimateCall call;
    call.requested_temperature = 25.0f;
    queued.ac.control(call);
    // A pending mode change away from Dry rejects adjustment requests.
    call.requested_temperature.reset();
    call.requested_mode = climate::CLIMATE_MODE_COOL;
    queued.ac.control(call);
    CHECK(!queued.ac.set_dry_offset(1));
}

void fresh_baseline_guards(int offset) {
    for (const auto frame : {dry_status(0x01, 0x28), dry_status(0x01, 0x30), dry_status(0x81)}) {
        Rig rig;
        rig.receive(dry_status(), 100);
        CHECK(rig.ac.set_dry_offset(offset));
        rig.loop(101);
        rig.receive(frame, 200);
        CHECK(rig.setters() == 0);
        CHECK(logged("Operation 1 PREREQUISITE") && rig.ac.warning);
    }
    Rig no_baseline;
    no_baseline.receive(dry_status(), 100);
    CHECK(no_baseline.ac.set_dry_offset(offset));
    for (uint32_t t = 101; t <= 11000; ++t) no_baseline.loop(t);
    CHECK(no_baseline.setters() == 0 && logged("Operation 1 TIMEOUT"));
}

void unconfirmed_no_retry(int offset) {
    for (bool respond : {false, true}) {
        Rig rig;
        rig.receive(dry_status(), 100);
        CHECK(rig.ac.set_dry_offset(offset));
        rig.loop(101);
        rig.receive(dry_status(), 200);
        for (uint32_t t = 201; t <= 12000; ++t) {
            if (respond && t % 100 == 0) rig.receive(dry_status(), t);
            else rig.loop(t);
        }
        CHECK(rig.setters() == 1);
        CHECK(logged(respond ? "Operation 1 EXPIRED" : "Operation 1 TIMEOUT"));
        CHECK(rig.ac.warning && std::isnan(rig.ac.target_temperature));
    }
    Rig changed_mode;
    changed_mode.receive(dry_status(), 100);
    CHECK(changed_mode.ac.set_dry_offset(1));
    changed_mode.loop(101);
    changed_mode.receive(dry_status(), 200);
    changed_mode.loop(300);
    changed_mode.loop(800);
    changed_mode.receive(dry_status(0x11, 0x28), 900);
    CHECK(changed_mode.setters() == 1 && logged("Operation 1 PREREQUISITE"));
}

struct TransportHost : transport::Listener {
    std::vector<Bytes> writes;
    std::vector<transport::Result> results;
    bool send_packet(const uint8_t *data, size_t size) override {
        writes.emplace_back(data, data + size);
        return true;
    }
    void operation_finished(uint32_t, transport::Result result, const transport::Request &) override {
        results.push_back(result);
    }
};

void transport_guards() {
    TransportHost host;
    transport::Engine engine(&host);
    engine.set_dry_offset_enabled(true);
    transport::Request request;
    request.fields = transport::DRY_OFFSET;
    request.dry_offset = 1;
    uint32_t generation;
    request.fields |= transport::TEMPERATURE;
    CHECK(!engine.enqueue(request, 0, generation));
    request.fields = transport::DRY_OFFSET;
    request.fahrenheit = true;
    CHECK(!engine.enqueue(request, 0, generation));
    request.fahrenheit = false;
    request.dry_offset = 8;
    CHECK(!engine.enqueue(request, 0, generation));
    request.dry_offset = -8;
    CHECK(!engine.enqueue(request, 0, generation));
    request.dry_offset = 1;
    CHECK(engine.enqueue(request, 0, generation));
    engine.tick(0);
    DeviceStatus status;
    CHECK(protocol::decode_status(dry_status().data(), 82, status));
    engine.receive(status, 100);
    // Remote mode or offset changes after the baseline invalidate the pre-send guard.
    status.mode_status = 2;
    engine.receive(status, 101);
    engine.tick(101);
    CHECK(host.writes.size() == 1 && host.writes[0][13] == 0x66);
    CHECK(host.results.back() == transport::Result::PREREQUISITE);

    TransportHost second_host;
    transport::Engine second(&second_host);
    second.set_dry_offset_enabled(true);
    CHECK(second.enqueue(request, 0, generation));
    second.tick(0);
    CHECK(protocol::decode_status(dry_status().data(), 82, status));
    second.receive(status, 100);
    status.temperature_compensation_raw = 0x21;
    second.receive(status, 101);
    second.tick(101);
    CHECK(second_host.writes.size() == 1);
    CHECK(second_host.results.back() == transport::Result::PREREQUISITE);

    Rig independent;
    independent.ac.set_dry_offset_number(nullptr);
    independent.receive(dry_status(), 100);
    CHECK(!independent.ac.set_dry_offset(1));
    CHECK(independent.setters() == 0);
}

void baseline_matrix() {
    for (int desired = -7; desired <= 7; ++desired) {
        for (int current = -7; current <= 7; ++current) {
            TransportHost host;
            transport::Engine engine(&host);
            engine.set_dry_offset_enabled(true);
            transport::Request request;
            request.fields = transport::DRY_OFFSET;
            request.dry_offset = static_cast<int8_t>(desired);
            uint32_t generation;
            CHECK(engine.enqueue(request, 0, generation));
            engine.tick(0);
            DeviceStatus status;
            const auto frame = dry_status(OFFSET_READBACK[current + 7]);
            CHECK(protocol::decode_status(frame.data(), frame.size(), status));
            engine.receive(status, 100);
            engine.tick(100);
            if (desired == current) {
                CHECK(host.writes.size() == 1 && host.results.back() == transport::Result::CONFIRMED);
            } else {
                CHECK(host.writes.size() == 2 && host.writes.back() == candidate(desired));
                CHECK(host.results.empty());
                engine.tick(200);
                engine.tick(700);
                engine.receive(status, 750);  // The old offset must not confirm the new one.
                CHECK(host.results.empty());
                status.temperature_compensation_raw = OFFSET_READBACK[desired + 7];
                engine.receive(status, 800);
                CHECK(host.results.size() == 1 && host.results.back() == transport::Result::CONFIRMED);
                CHECK(std::count_if(host.writes.begin(), host.writes.end(),
                                    [](const Bytes &packet) { return packet[13] == 0x65; }) == 1);
            }
        }
    }
    for (int desired : {-7, 0, 7}) {
        Rig unknown;
        unknown.receive(dry_status(0x81), 100);
        CHECK(unknown.ac.set_dry_offset(desired));
        unknown.loop(101);
        unknown.receive(dry_status(0x81), 200);
        CHECK(unknown.setters() == 0 && logged("Operation 1 PREREQUISITE"));
    }
}

void direct_sequence() {
    Rig rig;
    rig.receive(dry_status(), 100);
    int current = 0;
    uint32_t now = 100;
    unsigned writes = 0;
    for (int desired : {2, 3, 1, -1, -2, 0}) {
        CHECK(rig.ac.set_dry_offset(desired));
        rig.loop(now + 1);
        rig.receive(dry_status(OFFSET_READBACK[current + 7]), now + 100);
        CHECK(rig.setters() == ++writes && rig.bus.tx.back() == candidate(desired));
        rig.loop(now + 200);
        rig.loop(now + 700);
        rig.receive(dry_status(OFFSET_READBACK[desired + 7]), now + 800);
        CHECK(logged("Operation " + std::to_string(writes) +
                     " CONFIRMED: fields=0x80 dry_offset=" + std::to_string(desired)));
        CHECK(rig.ac.mode == climate::CLIMATE_MODE_DRY && std::isnan(rig.ac.target_temperature));
        current = desired;
        now += 800;
    }
}

void number_feedback() {
    Rig rig;
    CHECK(std::isnan(rig.adjustment.state));
    CHECK(!rig.adjustment.has_state());
    for (int offset = -7; offset <= 7; ++offset) {
        rig.receive(dry_status(OFFSET_READBACK[offset + 7]), 100 + (offset + 7) * 100);
        CHECK(rig.adjustment.state == offset);
        CHECK(rig.adjustment.has_state());
        CHECK(std::isnan(rig.ac.target_temperature));
    }
    const auto publications = rig.adjustment.publications.size();
    rig.receive(dry_status(0x71), 1600);
    CHECK(rig.adjustment.publications.size() == publications);
    // Device temperatures derived from an offset are not a writable climate target.
    auto derived = dry_status(0x71);
    derived[19] = 34;
    seal(derived);
    rig.receive(derived, 1700);
    CHECK(rig.adjustment.state == 7 && std::isnan(rig.ac.target_temperature));

    rig.receive(dry_status(0xB1, 0x28), 1800);  // Cool byte26 is not a signed Dry offset.
    CHECK(std::isnan(rig.adjustment.state) && rig.ac.target_temperature == 27);
    CHECK(!rig.adjustment.has_state());
    CHECK(!rig.ac.set_dry_offset(2));
    rig.receive(dry_status(0x11, 0x30), 1900);  // Off with retained Dry mode bits.
    CHECK(std::isnan(rig.adjustment.state) && std::isnan(rig.ac.target_temperature));
    rig.receive(dry_status(0x81), 2000);  // Unknown negative-zero encoding.
    CHECK(std::isnan(rig.adjustment.state));
    CHECK(!rig.adjustment.has_state());
    rig.receive(dry_status(0x91), 2100);
    CHECK(rig.adjustment.state == -1);
    auto unsupported = dry_status(0x71);
    unsupported[13] = 0x65;
    seal(unsupported);
    rig.receive(unsupported, 2200);
    CHECK(rig.adjustment.state == -1);
    auto invalid = dry_status(0x31);
    invalid[20] ^= 1;
    rig.receive(invalid, 2300);
    CHECK(rig.adjustment.state == -1);
    rig.loop(17100);  // Three missed polling intervals.
    CHECK(std::isnan(rig.adjustment.state));
    CHECK(!rig.adjustment.has_state());
    CHECK(!rig.ac.set_dry_offset(1));
    rig.receive(dry_status(0x21), 17200);
    CHECK(rig.adjustment.state == 2);
    test_clock = 40000;  // Reject stale feedback even before loop updates the warning.
    CHECK(!rig.ac.set_dry_offset(3));
}

void number_controls_and_queue() {
    Rig rig;
    rig.ac.set_optimistic(true);
    rig.receive(dry_status(), 100);
    const auto publications = rig.adjustment.publications.size();
    for (float value : {NAN, INFINITY, -INFINITY, 0.5f, -1.1f, 7.1f, -7.1f, 256.0f}) {
        CHECK(!rig.ac.set_dry_offset(value));
        CHECK(rig.adjustment.state == 0);
    }
    rig.set(1);
    CHECK(rig.adjustment.state == 0 && rig.adjustment.publications.size() == publications);
    rig.loop(101);
    rig.receive(dry_status(), 200);
    rig.set(3);
    rig.set(4);  // Replace the consecutive unsent request, not the in-flight packet.
    CHECK(logged("Operation 2 SUPERSEDED: fields=0x80 dry_offset=3"));
    CHECK(rig.setters() == 1 && rig.bus.tx.back() == candidate(1));
    rig.loop(300);
    rig.loop(800);
    rig.receive(dry_status(0x11), 900);
    CHECK(rig.adjustment.state == 1);
    rig.receive(dry_status(0x11), 1000);
    CHECK(rig.setters() == 2 && rig.bus.tx.back() == candidate(4));
    rig.loop(1100);
    rig.loop(1600);
    rig.receive(dry_status(0x41), 1700);
    CHECK(rig.adjustment.state == 4 && !rig.ac.warning);

    rig.set(-2);
    rig.loop(1701);
    rig.receive(dry_status(0x41), 1800);
    for (uint32_t now = 1801; now < 13000; ++now) {
        if (now % 100 == 0) rig.receive(dry_status(0x41), now);
        else rig.loop(now);
    }
    CHECK(rig.setters() == 3 && rig.adjustment.state == 4 && rig.ac.warning);
    CHECK(logged("Operation 4 EXPIRED"));

    Rig first, second;
    first.receive(dry_status(0x21), 100);
    second.receive(dry_status(0xC1), 100);
    first.set(7);
    CHECK(first.adjustment.state == 2 && second.adjustment.state == -4);
    CHECK(second.setters() == 0);
}

void temperature_gating() {
    Rig rig;
    rig.receive(dry_status(), 100);
    climate::ClimateCall target;
    target.requested_temperature = 24.0f;
    const auto before = rig.bus.tx.size();
    rig.ac.control(target);
    CHECK(rig.ac.warning && rig.bus.tx.size() == before);
    rig.loop(101);
    CHECK(rig.bus.tx.size() == before);

    // A combined request to enter Dry with an absolute target is rejected atomically.
    rig.receive(dry_status(0x01, 0x28), 200);
    target.requested_mode = climate::CLIMATE_MODE_DRY;
    rig.ac.control(target);
    rig.loop(201);
    CHECK(rig.setters() == 0 && rig.ac.mode == climate::CLIMATE_MODE_COOL);

    // Valid Cool target queued before a remote change to Dry must not be sent.
    target.requested_mode.reset();
    rig.ac.control(target);
    rig.loop(202);
    rig.receive(dry_status(), 300);
    CHECK(rig.setters() == 0 && logged("PREREQUISITE"));

    // Explicitly leaving Dry for Cool with a target remains supported.
    Rig leaving;
    leaving.receive(dry_status(), 100);
    target.requested_mode = climate::CLIMATE_MODE_COOL;
    leaving.ac.control(target);
    leaving.loop(101);
    leaving.receive(dry_status(), 200);
    CHECK(leaving.bus.tx.back() == Bytes(mode_cool, mode_cool + CMD_SIZE));
    leaving.loop(300);
    leaving.loop(800);
    leaving.receive(dry_status(0xB1, 0x28), 900);
    CommandPacket expected;
    CHECK(encode_temperature(24, false, expected));
    CHECK(leaving.bus.tx.back() == Bytes(expected.data, expected.data + expected.size));
    CHECK(std::isnan(leaving.adjustment.state));

    // Without the opt-in number, the historical Dry absolute-target path is unchanged.
    Rig legacy(false);
    legacy.receive(dry_status(), 100);
    target.requested_mode.reset();
    legacy.ac.control(target);
    legacy.loop(101);
    legacy.receive(dry_status(), 200);
    CHECK(legacy.bus.tx.back() == Bytes(expected.data, expected.data + expected.size));
    CHECK(legacy.ac.target_temperature == 27);
    CHECK(!legacy.ac.set_dry_offset(1));

    // Engine-level opt-in rejects unsupported offset requests by default.
    TransportHost host;
    transport::Engine disabled(&host);
    transport::Request offset;
    offset.fields = transport::DRY_OFFSET;
    uint32_t generation;
    CHECK(!disabled.enqueue(offset, 0, generation));

    // Even a change after the baseline but before transmission blocks a Dry absolute setter.
    TransportHost race_host;
    transport::Engine race(&race_host);
    race.set_dry_offset_enabled(true);
    transport::Request temperature;
    temperature.fields = transport::TEMPERATURE;
    temperature.temperature = 24;
    CHECK(race.enqueue(temperature, 0, generation));
    race.tick(0);
    DeviceStatus status;
    auto frame = dry_status(0xB1, 0x28);
    CHECK(protocol::decode_status(frame.data(), frame.size(), status));
    race.receive(status, 100);
    status.mode_status = 3;
    race.receive(status, 101);
    race.tick(101);
    CHECK(race_host.writes.size() == 1 && race_host.results.back() == transport::Result::PREREQUISITE);
}

void hardware_readback() {
    struct Evidence { const char *name; int offset; int target; };
    for (const auto evidence : {Evidence{"dry_minus_1.hex", -1, 28},
                               Evidence{"dry_minus_4.hex", -4, 23},
                               Evidence{"dry_plus_7.hex", 7, 34}}) {
        const auto frame = capture(evidence.name);
        DeviceStatus decoded;
        CHECK(protocol::decode_status(frame.data(), frame.size(), decoded));
        CHECK(decoded.indoor_temperature_setting == evidence.target);
        CHECK(decoded.temperature_compensation_raw == OFFSET_READBACK[evidence.offset + 7]);
        Rig rig;
        rig.receive(frame, 100);
        CHECK(rig.adjustment.has_state() && rig.adjustment.state == evidence.offset);
        CHECK(std::isnan(rig.ac.target_temperature) && rig.ac.mode == climate::CLIMATE_MODE_DRY);
        CHECK(rig.setters() == 0);
    }
}

int main() {
    encoding();
    for (int offset = -7; offset <= 7; ++offset) {
        if (offset == 0) continue;
        success_and_restore(offset);
        fresh_baseline_guards(offset);
        unconfirmed_no_retry(offset);
    }
    rejected_requests();
    transport_guards();
    baseline_matrix();
    direct_sequence();
    number_feedback();
    number_controls_and_queue();
    temperature_gating();
    hardware_readback();
}
