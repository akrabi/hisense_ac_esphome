#pragma once
#include "transport.h"

namespace esphome {
namespace hisense_ac {

// Per-field generations prevent an older completion from clearing a newer
// accepted setting, including calls that overlap on only some fields.
class PendingState {
public:
    void accept(const transport::Request &request, uint32_t generation, uint32_t now) {
        for (size_t i = 0; i < 6; ++i) {
            const uint8_t bit = 1U << i;
            if (!(request.fields & bit)) continue;
            copy_field_(values_, request, bit);
            fields_ |= bit;
            generations_[i] = generation;
            started_[i] = now;
        }
    }
    bool complete(uint32_t generation) {
        bool changed = false;
        for (size_t i = 0; i < 6; ++i) {
            const uint8_t bit = 1U << i;
            if ((fields_ & bit) && generations_[i] == generation) {
                fields_ &= ~bit;
                changed = true;
            }
        }
        return changed;
    }
    bool expire(uint32_t now) {
        bool changed = false;
        for (size_t i = 0; i < 6; ++i) {
            const uint8_t bit = 1U << i;
            if ((fields_ & bit) && static_cast<uint32_t>(now - started_[i]) >= transport::OPERATION_TIMEOUT_MS) {
                fields_ &= ~bit;
                changed = true;
            }
        }
        return changed;
    }
    uint8_t fields() const { return fields_; }
    transport::Request present(const transport::Request &confirmed, bool optimistic) const {
        auto result = confirmed;
        if (optimistic) {
            for (size_t i = 0; i < 6; ++i) {
                const uint8_t bit = 1U << i;
                if (fields_ & bit) {
                    copy_field_(result, values_, bit);
                    result.fields |= bit;
                }
            }
        }
        return result;
    }

private:
    static void copy_field_(transport::Request &out, const transport::Request &in, uint8_t field) {
        switch (field) {
            case transport::MODE: out.mode = in.mode; break;
            case transport::TEMPERATURE: out.temperature = in.temperature; break;
            case transport::FAN: out.fan = in.fan; break;
            case transport::SWING: out.swing = in.swing; break;
            case transport::PRESET: out.preset = in.preset; break;
            case transport::FIELD_DISPLAY: out.display = in.display; break;
        }
    }
    uint8_t fields_{0};
    transport::Request values_{};
    uint32_t generations_[6]{};
    uint32_t started_[6]{};
};

}  // namespace hisense_ac
}  // namespace esphome
