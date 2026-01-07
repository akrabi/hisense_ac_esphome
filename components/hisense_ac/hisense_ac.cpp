#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "hisense_ac.h"
#include "commands.h"

namespace esphome {
namespace hisense_ac {

HisenseAC::HisenseAC(uart::UARTComponent *parent) : PollingComponent(5000), uart::UARTDevice(parent) {}

void HisenseAC::set_temperature_unit(Temperature_Unit unit) { 
    this->temp_unit = unit; 
}

void HisenseAC::set_compressor_frequency(sensor::Sensor *sensor) { 
    compressor_frequency = sensor; 
}

void HisenseAC::set_compressor_frequency_setting(sensor::Sensor *sensor) { 
    compressor_frequency_setting = sensor; 
}

void HisenseAC::set_compressor_frequency_send(sensor::Sensor *sensor) { 
    compressor_frequency_send = sensor; 
}

void HisenseAC::set_outdoor_temperature(sensor::Sensor *sensor) { 
    outdoor_temperature = sensor; 
}

void HisenseAC::set_outdoor_condenser_temperature(sensor::Sensor *sensor) { 
    outdoor_condenser_temperature = sensor; 
}

void HisenseAC::set_compressor_exhaust_temperature(sensor::Sensor *sensor) { 
    compressor_exhaust_temperature = sensor; 
}

void HisenseAC::set_target_exhaust_temperature(sensor::Sensor *sensor) { 
    target_exhaust_temperature = sensor; 
}

void HisenseAC::set_indoor_pipe_temperature(sensor::Sensor *sensor) { 
    indoor_pipe_temperature = sensor; 
}

void HisenseAC::set_indoor_humidity_setting(sensor::Sensor *sensor) { 
    indoor_humidity_setting = sensor; 
}

void HisenseAC::set_indoor_humidity_status(sensor::Sensor *sensor) { 
    indoor_humidity_status = sensor; 
}

void HisenseAC::setup()
{
    if (compressor_frequency != nullptr)
    {
        compressor_frequency->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
    }
    if (compressor_frequency_setting != nullptr)
    {
        compressor_frequency_setting->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
    }
    if (compressor_frequency_send != nullptr)
    {
        compressor_frequency_send->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
    }
    if (outdoor_temperature != nullptr)
    {
        outdoor_temperature->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
    }
    if (outdoor_condenser_temperature != nullptr)
    {
        outdoor_condenser_temperature->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
    }
    if (compressor_exhaust_temperature != nullptr)
    {
        compressor_exhaust_temperature->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
    }
    if (target_exhaust_temperature != nullptr)
    {
        target_exhaust_temperature->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
    }
    if (indoor_pipe_temperature != nullptr)
    {
        indoor_pipe_temperature->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
    }
    if (indoor_humidity_setting != nullptr)
    {
        indoor_humidity_setting->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
    }
    if (indoor_humidity_status != nullptr)
    {
        indoor_humidity_status->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
    }
    if (compressor_frequency != nullptr)
    {
        compressor_frequency->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
    }
    if (compressor_frequency_setting != nullptr)
    {
        compressor_frequency_setting->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
    }
    if (compressor_frequency_send != nullptr)
    {
        compressor_frequency_send->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
    }
    if (outdoor_temperature != nullptr)
    {
        outdoor_temperature->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
    }
    if (outdoor_condenser_temperature != nullptr)
    {
        outdoor_condenser_temperature->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
    }
    if (compressor_exhaust_temperature != nullptr)
    {
        compressor_exhaust_temperature->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
    }
    if (target_exhaust_temperature != nullptr)
    {
        target_exhaust_temperature->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
    }
    if (indoor_pipe_temperature != nullptr)
    {
        indoor_pipe_temperature->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
    }
    if (indoor_humidity_setting != nullptr)
    {
        indoor_humidity_setting->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
    }
    if (indoor_humidity_status != nullptr)
    {
        indoor_humidity_status->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
    }
    request_update();
}

void HisenseAC::loop()
{
    int msg_size = 0;
    while (this->available())
    {
        msg_size = get_response(this->read(), uart_buf);
        if (msg_size > 0)
        {
            ESP_LOGD(
                "hisense_ac",
                "compf: %d compf_set: %d compf_snd: %d",
                ((Device_Status*)uart_buf)->compressor_frequency,
                ((Device_Status*)uart_buf)->compressor_frequency_setting,
                ((Device_Status*)uart_buf)->compressor_frequency_send);

            ESP_LOGD(
                "hisense_ac",
                "out_temp: %d out_cond_temp: %d comp_exh_temp: %d comp_exh_temp_tgt: %d",
                ((Device_Status*)uart_buf)->outdoor_temperature,
                ((Device_Status*)uart_buf)->outdoor_condenser_temperature,
                ((Device_Status*)uart_buf)->compressor_exhaust_temperature,
                ((Device_Status*)uart_buf)->target_exhaust_temperature);

            ESP_LOGD(
                "hisense_ac",
                "indoor_pipe_temp %d",
                ((Device_Status*)uart_buf)->indoor_pipe_temperature);

            ESP_LOGD(
                "hisense_ac",
                "indor_humid_set: %d indoor_humid: %d",
                ((Device_Status*)uart_buf)->indoor_humidity_setting,
                ((Device_Status*)uart_buf)->indoor_humidity_status);

            // Convert temperatures to celsius
            float tgt_temp = ((Device_Status*)uart_buf)->indoor_temperature_setting;
            float curr_temp = ((Device_Status*)uart_buf)->indoor_temperature_status;
            
            if (this->temp_unit == Temperature_Unit::CELSIUS && tgt_temp > 7 && tgt_temp < 33 ||
                this->temp_unit == Temperature_Unit::FAHRENHEIT && tgt_temp > 45 && tgt_temp < 91)
            {
                target_temperature = tgt_temp;
            }

            if (this->temp_unit == Temperature_Unit::CELSIUS && curr_temp > 1 && curr_temp < 49 ||
                this->temp_unit == Temperature_Unit::FAHRENHEIT && curr_temp > 34 && curr_temp < 120)
            {

                current_temperature = curr_temp;
            }

            // See if the system is actively running
            bool comp_running = false;
            if (((Device_Status*)uart_buf)->compressor_frequency > 0)
            {
                comp_running = true;
            }

            if (((Device_Status*)uart_buf)->left_right && ((Device_Status*)uart_buf)->up_down)
                swing_mode = climate::CLIMATE_SWING_BOTH;
            else if (((Device_Status*)uart_buf)->left_right)
                swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
            else if (((Device_Status*)uart_buf)->up_down)
                swing_mode = climate::CLIMATE_SWING_VERTICAL;
            else
                swing_mode = climate::CLIMATE_SWING_OFF;

            if (((Device_Status*)uart_buf)->run_status == 0)
            {
                mode = climate::CLIMATE_MODE_OFF;
                action = climate::CLIMATE_ACTION_OFF;
            }
            else if (((Device_Status*)uart_buf)->mode_status == 0)
            {
                mode = climate::CLIMATE_MODE_FAN_ONLY;
                action = climate::CLIMATE_ACTION_FAN;
            }
            else if (((Device_Status*)uart_buf)->mode_status == 1)
            {
                mode = climate::CLIMATE_MODE_HEAT;
                if (comp_running)
                {
                    action = climate::CLIMATE_ACTION_HEATING;
                }
                else
                {
                    action = climate::CLIMATE_ACTION_IDLE;
                }
            }
            else if (((Device_Status*)uart_buf)->mode_status == 2)
            {
                mode = climate::CLIMATE_MODE_COOL;
                if (comp_running)
                {
                    action = climate::CLIMATE_ACTION_COOLING;
                }
                else
                {
                    action = climate::CLIMATE_ACTION_IDLE;
                }
            }
            else if (((Device_Status*)uart_buf)->mode_status == 3)
            {
                mode = climate::CLIMATE_MODE_DRY;
                if (comp_running)
                {
                    action = climate::CLIMATE_ACTION_DRYING;
                }
                else
                {
                    action = climate::CLIMATE_ACTION_IDLE;
                }
            }

            if (((Device_Status*)uart_buf)->wind_status == 18)
            {
                fan_mode = climate::CLIMATE_FAN_HIGH;
            }
            else if (((Device_Status*)uart_buf)->wind_status == 14)
            {
                fan_mode = climate::CLIMATE_FAN_MEDIUM;
            }
            else if (((Device_Status*)uart_buf)->wind_status == 10)
            {
                fan_mode = climate::CLIMATE_FAN_LOW;
            }
            else if (((Device_Status*)uart_buf)->wind_status == 2)
            {
                fan_mode = climate::CLIMATE_FAN_QUIET;
            }
            else if (((Device_Status*)uart_buf)->wind_status == 0)
            {
                fan_mode = climate::CLIMATE_FAN_AUTO;
            }

            // Save target temperature since it gets messed up by the mode switch command
            if (this->mode == climate::CLIMATE_MODE_COOL && target_temperature > 0)
            {
                cool_tgt_temp = target_temperature;
            }
            else if (this->mode == climate::CLIMATE_MODE_HEAT && target_temperature > 0)
            {
                heat_tgt_temp = target_temperature;
            }
        }
    }

    // Send buffered data without inserting any new messages
    blocking_send(0, 0);
}

void HisenseAC::update()
{
    request_update();

    this->publish_state();

    // Update sensors
    set_sensor(compressor_frequency, ((Device_Status*)uart_buf)->compressor_frequency);
    set_sensor(compressor_frequency_setting, ((Device_Status*)uart_buf)->compressor_frequency_setting);
    set_sensor(compressor_frequency_send, ((Device_Status*)uart_buf)->compressor_frequency_send);
    set_sensor(outdoor_temperature, ((Device_Status*)uart_buf)->outdoor_temperature);
    set_sensor(outdoor_condenser_temperature, ((Device_Status*)uart_buf)->outdoor_condenser_temperature);
    set_sensor(compressor_exhaust_temperature, ((Device_Status*)uart_buf)->compressor_exhaust_temperature);
    set_sensor(target_exhaust_temperature, ((Device_Status*)uart_buf)->target_exhaust_temperature);
    set_sensor(indoor_pipe_temperature, ((Device_Status*)uart_buf)->indoor_pipe_temperature);
    set_sensor(indoor_humidity_setting, ((Device_Status*)uart_buf)->indoor_humidity_setting);
    set_sensor(indoor_humidity_status, ((Device_Status*)uart_buf)->indoor_humidity_status);
    save_target_temperture();
}

void HisenseAC::control(const climate::ClimateCall &call)
{
    ESP_LOGD("hisense_ac", "Received climate call with mode: %s, target_temp: %.1f, fan_mode: %s, swing_mode: %s",
             call.get_mode().has_value() ? climate::climate_mode_to_string(call.get_mode().value()) : LOG_STR("Unknown"),
             call.get_target_temperature().has_value() ? call.get_target_temperature().value() : 0.0f,
             call.get_fan_mode().has_value() ? climate::climate_fan_mode_to_string(call.get_fan_mode().value()) : LOG_STR("Unknown"),
             call.get_swing_mode().has_value() ? climate::climate_swing_mode_to_string(call.get_swing_mode().value()) : LOG_STR("Unknown"));

    if (call.get_mode().has_value())
    {
        save_target_temperture();

        // User requested mode change
        climate::ClimateMode md = *call.get_mode();

        if (md != climate::CLIMATE_MODE_OFF && this->mode == climate::CLIMATE_MODE_OFF)
        {
            blocking_send(on, sizeof(on));
        }

        switch (md)
        {
            case climate::CLIMATE_MODE_OFF:
                blocking_send(off, sizeof(off));
                break;
            case climate::CLIMATE_MODE_COOL:
                blocking_send(mode_cool, sizeof(mode_cool));
                set_temp(cool_tgt_temp);
                break;
            case climate::CLIMATE_MODE_HEAT:
                blocking_send(mode_heat, sizeof(mode_heat));
                set_temp(heat_tgt_temp);
                break;
            case climate::CLIMATE_MODE_FAN_ONLY:
                blocking_send(mode_fan, sizeof(mode_fan));
                break;
            case climate::CLIMATE_MODE_DRY:
                blocking_send(mode_dry, sizeof(mode_dry));
                break;
            default:
                break;
        }

        // Publish updated state
        this->mode = md;
        this->publish_state();
    }

    if (call.get_target_temperature().has_value())
    {
        // User requested target temperature change
        float temp = *call.get_target_temperature();

        set_temp(temp);

        // Send target temp to climate
        target_temperature = temp;
        this->publish_state();
    }

    if (call.get_fan_mode().has_value())
    {
        climate::ClimateFanMode fm = *call.get_fan_mode();
        switch (fm)
        {
        case climate::CLIMATE_FAN_AUTO:
            blocking_send(speed_auto, sizeof(speed_auto));
            break;
        case climate::CLIMATE_FAN_LOW:
            blocking_send(speed_low, sizeof(speed_low));
            break;
        case climate::CLIMATE_FAN_MEDIUM:
            blocking_send(speed_med, sizeof(speed_med));
            break;
        case climate::CLIMATE_FAN_HIGH:
            blocking_send(speed_max, sizeof(speed_max));
            break;
        case climate::CLIMATE_FAN_QUIET:
            blocking_send(speed_mute, sizeof(speed_mute));
            break;
        default:
            break;
        }
        fan_mode = fm;
        this->publish_state();
    }

    if (call.get_swing_mode().has_value())
    {
        climate::ClimateSwingMode sm = *call.get_swing_mode();

        if (sm == climate::CLIMATE_SWING_OFF)
        {
            if (climate::CLIMATE_SWING_BOTH == swing_mode)
            {
                blocking_send(vert_swing, sizeof(vert_swing));
                blocking_send(hor_swing, sizeof(hor_swing));
            }
            else if (climate::CLIMATE_SWING_VERTICAL == swing_mode)
            {
                blocking_send(vert_swing, sizeof(vert_swing));
            }
            else if (climate::CLIMATE_SWING_HORIZONTAL == swing_mode)
            {
                blocking_send(hor_swing, sizeof(hor_swing));
            }
        }
        else if (sm == climate::CLIMATE_SWING_BOTH)
        {
            if (climate::CLIMATE_SWING_OFF == swing_mode)
            {
                blocking_send(vert_swing, sizeof(vert_swing));
                blocking_send(hor_swing, sizeof(hor_swing));
            }
            else if (climate::CLIMATE_SWING_VERTICAL == swing_mode)
            {
                blocking_send(hor_swing, sizeof(hor_swing));
            }
            else if (climate::CLIMATE_SWING_HORIZONTAL == swing_mode)
            {
                blocking_send(vert_swing, sizeof(vert_swing));
            }
        }
        else if (sm == climate::CLIMATE_SWING_VERTICAL)
        {
            if (climate::CLIMATE_SWING_BOTH == swing_mode)
            {
                blocking_send(hor_swing, sizeof(hor_swing));
            }
            else if (climate::CLIMATE_SWING_HORIZONTAL == swing_mode)
            {
                blocking_send(hor_swing, sizeof(hor_swing));
                blocking_send(vert_swing, sizeof(vert_swing));
            }
        }
        else if (sm == climate::CLIMATE_SWING_HORIZONTAL)
        {
            if (climate::CLIMATE_SWING_BOTH == swing_mode)
            {
                blocking_send(vert_swing, sizeof(vert_swing));
            }
            else if (climate::CLIMATE_SWING_VERTICAL == swing_mode)
            {
                blocking_send(vert_swing, sizeof(vert_swing));
                blocking_send(hor_swing, sizeof(hor_swing));
            }
        }

        swing_mode = sm;
        this->publish_state();
    }

    if (call.get_preset().has_value())
    {
        climate::ClimatePreset pre = *call.get_preset();
        switch (pre)
        {
        case climate::CLIMATE_PRESET_NONE:
            blocking_send(turbo_off, sizeof(turbo_off));
            blocking_send(energysave_off, sizeof(energysave_off));
            break;
        case climate::CLIMATE_PRESET_BOOST:
            blocking_send(turbo_on, sizeof(turbo_on));
            break;
        case climate::CLIMATE_PRESET_ECO:
            blocking_send(energysave_on, sizeof(energysave_on));
            break;
        default:
            break;
        }

        preset = pre;
        this->publish_state();
    }
}

void HisenseAC::save_target_temperture()
{
    // Save target temperature since it gets messed up by the mode switch command
    if (this->mode == climate::CLIMATE_MODE_COOL && target_temperature > 0)
    {
        cool_tgt_temp = target_temperature;
    }
    else if (this->mode == climate::CLIMATE_MODE_HEAT && target_temperature > 0)
    {
        heat_tgt_temp = target_temperature;
    }
}

climate::ClimateTraits HisenseAC::traits()
{
    // The capabilities of the climate device
    climate::ClimateTraits traits = climate::ClimateTraits();
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_ACTION);
    traits.set_visual_min_temperature(16);
    traits.set_visual_max_temperature(30);
    traits.set_visual_temperature_step(1);
    traits.set_supported_modes({
        climate::CLIMATE_MODE_OFF,
        climate::CLIMATE_MODE_COOL,
        climate::CLIMATE_MODE_HEAT,
        climate::CLIMATE_MODE_FAN_ONLY,
        climate::CLIMATE_MODE_DRY,
    });
    traits.set_supported_swing_modes({climate::CLIMATE_SWING_OFF,
                                        climate::CLIMATE_SWING_BOTH,
                                        climate::CLIMATE_SWING_VERTICAL,
                                        climate::CLIMATE_SWING_HORIZONTAL});
    traits.set_supported_fan_modes({
        climate::CLIMATE_FAN_AUTO,
        climate::CLIMATE_FAN_LOW,
        climate::CLIMATE_FAN_MEDIUM,
        climate::CLIMATE_FAN_HIGH,
        climate::CLIMATE_FAN_QUIET,
    });
    traits.set_supported_presets({climate::CLIMATE_PRESET_NONE,
                                    climate::CLIMATE_PRESET_BOOST,
                                    climate::CLIMATE_PRESET_ECO});
    return traits;
}



// Handle bytes form the UART to build a complete message
int HisenseAC::get_response(const uint8_t input, uint8_t *out)
{
    static char buf[UART_BUF_SIZE] = {0};
    static int buf_idx = 0;
    static int msg_size = 0;
    static uint16_t checksum = 0;
    static bool f4_detect = false;
    bool reset = false;

    // Put the byte in the buffer
    if (!f4_detect)
        buf[buf_idx++] = input;
    else
        f4_detect = false;

    // The checksum is computed from byte index 2 to msg_size - 4
    if ((buf_idx > 2 && buf_idx < 6) || (buf_idx < msg_size - 4))
    {
        checksum += buf[buf_idx - 1];
    }

    // Make sure we don't ever overflow the buffer
    if (buf_idx >= UART_BUF_SIZE) 
    {
        reset = true;
    }
    else if (buf_idx == 1) // Search for frame start byte 1
    {
        if (input != 0xF4)
        {
            reset = true;
        }
    }
    else if (buf_idx == 2) // Search for frame start byte 2
    {
        if (input != 0xF5)
        {
            reset = true;
        }
    }
    else if (buf_idx == 3) // Search for message mode byte (1 = repsonse)
    {
        if (input != 0x01)
        {
            reset = true;
        }
    }
    else if (buf_idx == 4) // Search for message type (we only handle 0x40)
    {
        if (input != 0x40)
        {
            reset = true;
        }
    }
    else if (buf_idx == 5) // get message size
    {
        msg_size = input + 9; // add header and footer bytes + this byte
    }
    else if (buf_idx == msg_size - 2) // third to last byte (end of checksum)
    {
        uint16_t rxd_checksum = buf[msg_size - 4];
        rxd_checksum = rxd_checksum << 8;
        rxd_checksum |= buf[msg_size - 3];
        if (rxd_checksum != checksum)
        {
            ESP_LOGE(
                "hisense_ac",
                "CRC check failed. Computed: %d Received: %d",
                checksum,
                rxd_checksum);
            reset = true;
        }
    }
    else if (buf_idx == msg_size - 1) // second to last byte
    {
        if (input != 0xF4)
        {
            reset = true;
        }
    }
    else if (buf_idx == msg_size) // last byte
    {
        if (input != 0xFB)
        {
            reset = true;
        }
        else
        {
            int msg_size_cpy = msg_size;
            ESP_LOGD(
                "hisense_ac",
                "Received %d bytes.",
                msg_size);
            memcpy(out, buf, msg_size);
            buf_idx = 0;
            msg_size = 0;
            checksum = 0;     
            wait_for_rx = false;              
            return msg_size_cpy;
        }
    }
    else if (!f4_detect && input == 0xF4)
    {
        f4_detect = true;
    }

    // Reset the static variables if we failed any of the conditions.
    if (reset)
    {
        ESP_LOGD("hisense_ac", "Resetting RX buffer.");
        buf_idx = 0;
        msg_size = 0;
        checksum = 0;
        wait_for_rx = false;       
    }

    return 0;
}

// This function buffers messages to be sent to the AC.
// Send messages one at a time and wait for each acknowledgement.
void HisenseAC::blocking_send(uint8_t buf[], size_t sz)
{
    static uint8_t insert = 0;
    static uint8_t read = 0;
    static uint8_t hold_buf[8][64] = {0};
    static size_t sz_buf[8] = {0,0,0,0,0,0,0,0};
    static uint8_t num_buffered = 0;
    static uint32_t start_time = 0;

    if (sz > 0)
    {
        ESP_LOGD("hisense_ac", 
                "Buffering message. Read: %d Insert: %d",
                read,
                insert);
        memcpy(hold_buf[insert], buf, sz);
        sz_buf[insert] = sz;
        insert = (insert + 1) & 7;
        if (num_buffered == 0)
        {
            start_time = millis();
        }
        num_buffered++;   
    }

    // Return if we're waiting. If we get to seven messages buffered, 
    // there's probably an issue and we shouldn't block anymore.
    if (num_buffered < 8 && wait_for_rx && (millis() - start_time < 500))
    {
        return;
    }

    if (sz_buf[read] > 0)
    {
        ESP_LOGD("hisense_ac", 
                "Sending message. Read: %d Insert: %d",
                read,
                insert);
        uart::UARTDevice::write_array(hold_buf[read], sz_buf[read]);
        uart::UARTDevice::flush();
        sz_buf[read] = 0;
        read = (read + 1) & 7;
        wait_for_rx = true;
        num_buffered--;
        start_time = millis();
    }
}

// Get status from the AC
void HisenseAC::request_update()
{
    uint8_t req_stat[] = {
        0xF4, 0xF5, 0x00, 0x40,
        0x0C, 0x00, 0x00, 0x01,
        0x01, 0xFE, 0x01, 0x00,
        0x00, 0x66, 0x00, 0x00,
        0x00, 0x01, 0xB3, 0xF4,
        0xFB};

    ESP_LOGD("hisense_ac", "Requesting update.");
    blocking_send(req_stat, sizeof(req_stat));
}

// Update sensors when the value has actually changed.
void HisenseAC::set_sensor(sensor::Sensor *sensor, float value)
{
    if (sensor != nullptr && (!sensor->has_state() || sensor->get_raw_state() != value))
    {
        sensor->publish_state(value);
    }
}

// Set the temperature
void HisenseAC::set_temp(float temp)
{
    uint8_t rounded_temp = roundf(temp);
    if (this->temp_unit == Temperature_Unit::CELSIUS)
    {
        switch (rounded_temp)
        {
            case 16:
                blocking_send(temp_16_C, sizeof(temp_16_C));
                break;
            case 17:
                blocking_send(temp_17_C, sizeof(temp_17_C));
                break;
            case 18:
                blocking_send(temp_18_C, sizeof(temp_18_C));
                break;
            case 19:
                blocking_send(temp_19_C, sizeof(temp_19_C));
                break;
            case 20:
                blocking_send(temp_20_C, sizeof(temp_20_C));
                break;
            case 21:
                blocking_send(temp_21_C, sizeof(temp_21_C));
                break;
            case 22:
                blocking_send(temp_22_C, sizeof(temp_22_C));
                break;
            case 23:
                blocking_send(temp_23_C, sizeof(temp_23_C));
                break;
            case 24:
                blocking_send(temp_24_C, sizeof(temp_24_C));
                break;
            case 25:
                blocking_send(temp_25_C, sizeof(temp_25_C));
                break;
            case 26:
                blocking_send(temp_26_C, sizeof(temp_26_C));
                break;
            case 27:
                blocking_send(temp_27_C, sizeof(temp_27_C));
                break;
            case 28:
                blocking_send(temp_28_C, sizeof(temp_28_C));
                break;
            case 29:
                blocking_send(temp_29_C, sizeof(temp_29_C));
                break;
            case 30:
                blocking_send(temp_30_C, sizeof(temp_30_C));
                break;
            case 31:
                blocking_send(temp_31_C, sizeof(temp_31_C));
                break;
            case 32:
                blocking_send(temp_32_C, sizeof(temp_32_C));
                break;
            default:
                break;
        }
    }
    else if (this->temp_unit == Temperature_Unit::FAHRENHEIT)
    {
        switch (rounded_temp)
        {
            case 61:
                blocking_send(temp_61_F, sizeof(temp_61_F));
                break;
            case 62:
                blocking_send(temp_62_F, sizeof(temp_62_F));
                break;
            case 63:
                blocking_send(temp_63_F, sizeof(temp_63_F));
                break;
            case 64:
                blocking_send(temp_64_F, sizeof(temp_64_F));
                break;
            case 65:
                blocking_send(temp_65_F, sizeof(temp_65_F));
                break;
            case 66:
                blocking_send(temp_66_F, sizeof(temp_66_F));
                break;
            case 67:
                blocking_send(temp_67_F, sizeof(temp_67_F));
                break;
            case 68:
                blocking_send(temp_68_F, sizeof(temp_68_F));
                break;
            case 69:
                blocking_send(temp_69_F, sizeof(temp_69_F));
                break;
            case 70:
                blocking_send(temp_70_F, sizeof(temp_70_F));
                break;
            case 71:
                blocking_send(temp_71_F, sizeof(temp_71_F));
                break;
            case 72:
                blocking_send(temp_72_F, sizeof(temp_72_F));
                break;
            case 73:
                blocking_send(temp_73_F, sizeof(temp_73_F));
                break;
            case 74:
                blocking_send(temp_74_F, sizeof(temp_74_F));
                break;
            case 75:
                blocking_send(temp_75_F, sizeof(temp_75_F));
                break;
            case 76:
                blocking_send(temp_76_F, sizeof(temp_76_F));
                break;
            case 77:
                blocking_send(temp_77_F, sizeof(temp_77_F));
                break;
            case 78:
                blocking_send(temp_78_F, sizeof(temp_78_F));
                break;
            case 79:
                blocking_send(temp_79_F, sizeof(temp_79_F));
                break;
            case 80:
                blocking_send(temp_80_F, sizeof(temp_80_F));
                break;
            case 81:
                blocking_send(temp_81_F, sizeof(temp_81_F));
                break;
            case 82:
                blocking_send(temp_82_F, sizeof(temp_82_F));
                break;
            case 83:
                blocking_send(temp_83_F, sizeof(temp_83_F));
                break;
            case 84:
                blocking_send(temp_84_F, sizeof(temp_84_F));
                break;
            case 85:
                blocking_send(temp_85_F, sizeof(temp_85_F));
                break;
            case 86:
                blocking_send(temp_86_F, sizeof(temp_86_F));
                break;
            case 87:
                blocking_send(temp_87_F, sizeof(temp_87_F));
                break;
            case 88:
                blocking_send(temp_88_F, sizeof(temp_88_F));
                break;
            case 89:
                blocking_send(temp_89_F, sizeof(temp_89_F));
                break;
            case 90:    
                blocking_send(temp_90_F, sizeof(temp_90_F));
                break;
            default:
                break;
        }
    }        
}

} // namespace hisense_ac
} // namespace esphome
