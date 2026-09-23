#include "transport.h"
#include "commands.h"
#include "temperature.h"
#include <initializer_list>

namespace esphome {
namespace hisense_ac {
namespace transport {
namespace {
const uint8_t STATUS_QUERY[] = {
    0xF4,0xF5,0x00,0x40,0x0C,0x00,0x00,0x01,0x01,0xFE,0x01,
    0x00,0x00,0x66,0x00,0x00,0x00,0x01,0xB3,0xF4,0xFB
};
Request field(uint8_t mask, const Request &source) {
    Request result = source;
    result.fields = mask;
    return result;
}
uint8_t swing(const DeviceStatus &status) {
    return (status.left_right ? 1 : 0) | (status.up_down ? 2 : 0);
}
}  // namespace

bool Engine::matches(const DeviceStatus &s, const Request &r) {
    if ((r.fields & POWER) && s.run_status == 0) return false;
    if (r.fields & MODE) {
        if (r.mode == MODE_OFF ? s.run_status != 0 : (s.run_status == 0 || s.mode_status != r.mode))
            return false;
    }
    if ((r.fields & TEMPERATURE) &&
        std::fabs(temperature::from_device(s.indoor_temperature_setting, r.fahrenheit) - r.temperature) > 0.001f)
        return false;
    if ((r.fields & FAN) && normalize_fan_status(s.wind_status) != r.fan) return false;
    if ((r.fields & SWING) && swing(s) != r.swing) return false;
    if ((r.fields & FIELD_DISPLAY) && s.back_led != r.display) return false;
    if ((r.fields & DRY_OFFSET) &&
        (s.run_status == 0 || s.mode_status != 3 ||
         (s.temperature_compensation_raw >> 4) != dry_offset_nibble(r.dry_offset)))
        return false;
    return (r.fields & PRESET) == 0;  // No verified feedback mapping.
}

bool Engine::enqueue(const Request &request, uint32_t now, uint32_t &generation) {
    auto normalized = request;
    uint8_t encoded_temperature;
    if (request.fields == 0 || (request.fields & ~(MODE|TEMPERATURE|FAN|SWING|PRESET|FIELD_DISPLAY|DRY_OFFSET)) ||
        ((request.fields & DRY_OFFSET) &&
         (!dry_offset_enabled_ || request.fields != DRY_OFFSET ||
          request.dry_offset < -7 || request.dry_offset > 7 || request.fahrenheit)) ||
        ((request.fields & MODE) && request.mode > MODE_OFF) ||
        ((request.fields & SWING) && request.swing > 3) ||
        ((request.fields & PRESET) && request.preset > 2) ||
        ((request.fields & FAN) && request.fan != 0 && request.fan != 2 &&
         request.fan != 10 && request.fan != 14 && request.fan != 18) ||
        ((request.fields & TEMPERATURE) && !temperature::normalize(
            request.temperature, request.fahrenheit, normalized.temperature, encoded_temperature)))
        return false;
    const bool absolute = request.fields == TEMPERATURE || request.fields == FAN ||
                          request.fields == FIELD_DISPLAY || request.fields == DRY_OFFSET;
    const bool replace = count_ != 0 && absolute && queue_[count_ - 1].request.fields == request.fields;
    if (!replace && count_ == QUEUE_CAPACITY) return false;
    if (++next_generation_ == 0) ++next_generation_;
    generation = next_generation_;
    if (replace) {
        const auto old = queue_[--count_];
        listener_->operation_finished(old.generation, Result::SUPERSEDED, old.request);
    }
    queue_[count_++] = {normalized, generation, now};
    return true;
}

void Engine::request_poll() {
    if (phase_ != Phase::POLL_DRAIN && phase_ != Phase::WAIT_STATUS)
        poll_requested_ = true;
}

bool Engine::add_step_(const uint8_t *data, size_t size, Request expected, Request guard) {
    if (size > MAX_PACKET_SIZE || size == 0 || step_count_ == MAX_STEPS) return false;
    steps_[step_count_++] = {data, size, expected, guard};
    return true;
}

bool Engine::build_steps_() {
    step_ = step_count_ = 0;
    unverified_ = false;
    const auto &r = current_.request;
    Request mode_guard;
    if (r.fields & MODE) {
        mode_guard = field(MODE, r);
        if (r.mode != MODE_OFF && status_.run_status == 0 &&
            !add_step_(on, sizeof(on), field(POWER, r))) return false;
        if (!matches(status_, mode_guard)) {
            const uint8_t *commands[] = {mode_fan, mode_heat, mode_cool, mode_dry, off};
            if (!add_step_(commands[r.mode], CMD_SIZE, mode_guard)) return false;
        }
    }
    if (r.fields & TEMPERATURE) {
        // Always restore after a mode command, even if the old setpoint matches.
        if ((r.fields & MODE) || !matches(status_, field(TEMPERATURE, r))) {
            uint8_t encoded;
            float normalized;
            if (!temperature::normalize(r.temperature, r.fahrenheit, normalized, encoded)) return false;
            if (!encode_temperature(encoded, r.fahrenheit, temperature_packet_)) return false;
            if (!add_step_(temperature_packet_.data, temperature_packet_.size,
                           field(TEMPERATURE, r), mode_guard)) return false;
        }
    }
    if ((r.fields & FAN) && !matches(status_, field(FAN, r))) {
        const uint8_t *data = r.fan == 0 ? speed_auto : r.fan == 2 ? speed_mute :
                              r.fan == 10 ? speed_low : r.fan == 14 ? speed_med : speed_max;
        if (!add_step_(data, CMD_SIZE, field(FAN, r), mode_guard)) return false;
    }
    if (r.fields & SWING) {
        Request before = field(SWING, r);
        before.swing = swing(status_);
        // Preserve the existing horizontal-to-vertical command order.
        const uint8_t first_axis = before.swing == 1 && r.swing == 2 ? 1 : 2;
        const uint8_t axes[] = {first_axis, static_cast<uint8_t>(first_axis ^ 3)};
        for (uint8_t axis : axes) {
            if ((before.swing ^ r.swing) & axis) {
                auto after = before;
                after.swing ^= axis;
                if (!add_step_(axis == 2 ? vert_swing : hor_swing, CMD_SIZE, after, before)) return false;
                before = after;
            }
        }
    }
    if (r.fields & PRESET) {
        unverified_ = true;
        if (!add_step_(r.preset == 1 ? turbo_on : r.preset == 2 ? energysave_on : turbo_off, CMD_SIZE, {}))
            return false;
        if (r.preset == 0 && !add_step_(energysave_off, CMD_SIZE, {})) return false;
    }
    if ((r.fields & FIELD_DISPLAY) && !matches(status_, field(FIELD_DISPLAY, r)) &&
        !add_step_(r.display ? display_on : display_off, CMD_SIZE, field(FIELD_DISPLAY, r))) return false;
    return true;
}

bool Engine::send_(const uint8_t *data, size_t size, uint32_t now) {
    if (size > MAX_PACKET_SIZE || (tx_started_ && !due_(now, tx_until_))) return false;
    if (!listener_->send_packet(data, size)) return false;
    tx_started_ = true;
    tx_until_ = now + wire_time_ms(size);
    return true;
}

void Engine::poll_(uint32_t now) {
    poll_requested_ = false;
    response_seen_ = false;
    if (active_ && !baseline_poll_) ++confirmation_polls_;
    if (!send_(STATUS_QUERY, sizeof(STATUS_QUERY), now)) {
        finish_(Result::WRITE_FAILED, now);
        return;
    }
    deadline_ = tx_until_ + RESPONSE_TIMEOUT_MS;
    phase_ = Phase::POLL_DRAIN;
}

void Engine::cancel_pending_(Result result) {
    const size_t count = count_;
    count_ = 0;
    for (size_t i = 0; i < count; ++i)
        listener_->operation_finished(queue_[i].generation, result, queue_[i].request);
}

void Engine::finish_(Result result, uint32_t now) {
    const bool failure = result != Result::CONFIRMED && result != Result::UNVERIFIED;
    if (active_) {
        active_ = false;
        listener_->operation_finished(current_.generation, result, current_.request);
    } else if (failure) {
        listener_->operation_finished(0, result, {});
    }
    if (failure) {
        cancel_pending_(Result::CANCELLED);
        // One read-only recovery poll; never retry a setter or toggle.
        phase_ = recovering_ ? Phase::IDLE : Phase::RECOVERY;
        deadline_ = now + RESPONSE_TIMEOUT_MS;
        poll_requested_ = false;
        recovering_ = !recovering_;
    } else {
        phase_ = Phase::IDLE;
        recovering_ = false;
    }
}

void Engine::tick(uint32_t now) {
    for (size_t i = 0; i < count_;) {
        if (static_cast<uint32_t>(now - queue_[i].queued_at) < OPERATION_TIMEOUT_MS) { ++i; continue; }
        const auto expired = queue_[i];
        for (size_t j = i + 1; j < count_; ++j) queue_[j - 1] = queue_[j];
        --count_;
        listener_->operation_finished(expired.generation, Result::EXPIRED, expired.request);
    }
    if (active_ && static_cast<uint32_t>(now - current_.queued_at) >= OPERATION_TIMEOUT_MS) {
        finish_(Result::EXPIRED, now);
        return;
    }
    switch (phase_) {
        case Phase::IDLE:
            if (tx_started_ && !due_(now, tx_until_)) return;
            if (poll_requested_) { baseline_poll_ = false; poll_(now); }
            else if (count_ != 0) {
                current_ = queue_[0];
                for (size_t i = 1; i < count_; ++i) queue_[i - 1] = queue_[i];
                --count_;
                active_ = true;
                baseline_poll_ = true;
                poll_(now);
            }
            break;
        case Phase::CONTROL_READY:
            if (tx_started_ && !due_(now, tx_until_)) return;
            if (dry_offset_enabled_ && (steps_[step_].expected.fields & TEMPERATURE) &&
                status_.mode_status == 3) {
                finish_(Result::PREREQUISITE, now);
                return;
            }
            if (!matches(status_, steps_[step_].guard)) { finish_(Result::PREREQUISITE, now); return; }
            if (!send_(steps_[step_].data, steps_[step_].size, now)) {
                finish_(Result::WRITE_FAILED, now); return;
            }
            phase_ = Phase::CONTROL_DRAIN;
            deadline_ = tx_until_ + RESPONSE_TIMEOUT_MS;
            break;
        case Phase::CONTROL_DRAIN:
            if (due_(now, tx_until_)) phase_ = Phase::CONTROL_SETTLE;
            break;
        case Phase::CONTROL_SETTLE:
            if (due_(now, deadline_)) { baseline_poll_ = false; poll_(now); }
            break;
        case Phase::POLL_DRAIN:
            if (due_(now, tx_until_)) phase_ = Phase::WAIT_STATUS;
            break;
        case Phase::WAIT_STATUS:
            if (due_(now, deadline_)) {
                // A stale status is not rejection. Re-read (never resend the
                // setter) within the operation's 10 s confirmation lifetime.
                if (active_ && !baseline_poll_ && response_seen_ && confirmation_polls_ < 20)
                    poll_(now);
                else
                    finish_(Result::TIMEOUT, now);
            }
            break;
        case Phase::RECOVERY:
            if (due_(now, deadline_) && (!tx_started_ || due_(now, tx_until_))) {
                baseline_poll_ = false;
                poll_(now);
            }
            break;
    }
}

void Engine::receive(const DeviceStatus &status, uint32_t now) {
    status_ = status;
    if (phase_ == Phase::POLL_DRAIN && due_(now, tx_until_)) phase_ = Phase::WAIT_STATUS;
    if (phase_ != Phase::WAIT_STATUS || due_(now, deadline_)) return;
    response_seen_ = true;
    if (!active_) { finish_(Result::CONFIRMED, now); return; }
    if (static_cast<uint32_t>(now - current_.queued_at) >= OPERATION_TIMEOUT_MS) return;
    if (baseline_poll_) {
        confirmation_polls_ = 0;
        const auto &request = current_.request;
        if (dry_offset_enabled_ && (request.fields & TEMPERATURE) &&
            ((request.fields & MODE) ? request.mode == 3 : status.mode_status == 3)) {
            finish_(Result::PREREQUISITE, now);
            return;
        }
        if (current_.request.fields == DRY_OFFSET) {
            const auto &r = current_.request;
            // Require fresh active Dry status; this command must never change mode or power.
            const uint8_t nibble = status.temperature_compensation_raw >> 4;
            if (!dry_offset_enabled_ || status.run_status == 0 || status.mode_status != 3 || nibble == 0x08) {
                finish_(Result::PREREQUISITE, now);
                return;
            }
            step_ = step_count_ = 0;
            unverified_ = false;
            if (matches(status, r)) {
                finish_(Result::CONFIRMED, now);  // Already matches; no setter needed.
                return;
            }
            auto guard = r;
            guard.dry_offset = nibble & 0x08 ? -static_cast<int8_t>(nibble & 0x07) : static_cast<int8_t>(nibble);
            if (!encode_dry_offset(r.dry_offset, temperature_packet_) ||
                !add_step_(temperature_packet_.data, temperature_packet_.size, r, guard)) {
                finish_(Result::UNSUPPORTED, now);
                return;
            }
            phase_ = Phase::CONTROL_READY;
            return;
        }
        if (!build_steps_()) { finish_(Result::UNSUPPORTED, now); return; }
        if (step_count_ == 0) { finish_(Result::CONFIRMED, now); return; }
        phase_ = Phase::CONTROL_READY;
    } else if (current_.request.fields == DRY_OFFSET &&
               (status.run_status == 0 || status.mode_status != 3)) {
        finish_(Result::PREREQUISITE, now);
    } else if (matches(status, steps_[step_].expected)) {
        if (step_ + 1 == step_count_) {
            auto final = current_.request;
            final.fields &= ~PRESET;
            if (!matches(status, final)) return;
            finish_(unverified_ ? Result::UNVERIFIED : Result::CONFIRMED, now);
        } else {
            ++step_;
            confirmation_polls_ = 0;
            phase_ = Phase::CONTROL_READY;
        }
    }
}

}  // namespace transport
}  // namespace hisense_ac
}  // namespace esphome
