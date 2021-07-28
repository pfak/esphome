#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace daikin {

enum Model {
  MODEL_ARC470A1 = 0,  /// Temperature range is from 10 to 30
  MODEL_ARC432A14 = 1,  /// Temperature range is from 18 to 32
};


const uint32_t DAIKIN_BOOST_TIMEOUT = 10*60*1000;

// Temperature
const uint8_t DAIKIN_ARC470A1_TEMP_MAX = 30;
const uint8_t DAIKIN_ARC470A1_TEMP_MIN = 10;
const uint8_t DAIKIN_ARC432A14_TEMP_MAX = 32;
const uint8_t DAIKIN_ARC432A14_TEMP_MIN = 18;

// Modes
const uint8_t DAIKIN_MODE_AUTO = 0x00;
const uint8_t DAIKIN_MODE_COOL = 0x30;
const uint8_t DAIKIN_MODE_HEAT = 0x40;
const uint8_t DAIKIN_MODE_DRY = 0x20;
const uint8_t DAIKIN_MODE_FAN = 0x60;
const uint8_t DAIKIN_MODE_OFF = 0x00;
const uint8_t DAIKIN_MODE_ON = 0x01;

// Fan Speed
const uint8_t DAIKIN_FAN_AUTO = 0xA0;
const uint8_t DAIKIN_FAN_SILENT = 0xB0;
const uint8_t DAIKIN_FAN_1 = 0x30;
const uint8_t DAIKIN_FAN_2 = 0x40;
const uint8_t DAIKIN_FAN_3 = 0x50;
const uint8_t DAIKIN_FAN_4 = 0x60;
const uint8_t DAIKIN_FAN_5 = 0x70;

// IR Transmission
const uint32_t DAIKIN_IR_FREQUENCY = 38000;
const uint32_t DAIKIN_HEADER_MARK = 3360;
const uint32_t DAIKIN_HEADER_SPACE = 1760;
const uint32_t DAIKIN_BIT_MARK = 520;
const uint32_t DAIKIN_ONE_SPACE = 1370;
const uint32_t DAIKIN_ZERO_SPACE = 360;
const uint32_t DAIKIN_MESSAGE_SPACE = 32300;
const uint32_t DAIKIN_MESSAGE_SHORT_SPACE = 25000;

// Frame sizes
static const uint8_t DAIKIN_HEADER_SIZE = 4;
static const uint8_t DAIKIN_MAX_FRAME_SIZE = 19;
static const uint8_t DAIKIN_FIRST_FRAME_SIZE = DAIKIN_HEADER_SIZE + 4;
static const uint8_t DAIKIN_TIME_FRAME_OFFSET = 8;
static const uint8_t DAIKIN_TIME_FRAME_SIZE = DAIKIN_HEADER_SIZE + 4;
static const uint8_t DAIKIN_STATE_FRAME_OFFSET = 16;
static const uint8_t DAIKIN_STATE_FRAME_SIZE = DAIKIN_HEADER_SIZE + 15;

// Frame headers
const uint8_t DAIKIN_FIRST_FRAME_HEADER = 0xC5;
const uint8_t DAIKIN_TIME_FRAME_HEADER = 0x42;
const uint8_t DAIKIN_STATE_FRAME_HEADER = 0x00;

// State frame offsets
static const uint8_t HEADER_ID = 3;
static const uint8_t MESSAGE_ID = 4;
// MODE_POWER_TIMERS_ID = 5
static const uint8_t COMFORT_ID = 6;
static const uint8_t SWING_VERTICAL_ID = (DAIKIN_STATE_FRAME_OFFSET + 8);
static const uint8_t SWING_HORIZONTAL_ID = (DAIKIN_STATE_FRAME_OFFSET + 9);
static const uint8_t TIMER_A_ID = (DAIKIN_STATE_FRAME_OFFSET + 10);
static const uint8_t TIMER_B_ID = (DAIKIN_STATE_FRAME_OFFSET + 11);
static const uint8_t TIMER_C_ID = (DAIKIN_STATE_FRAME_OFFSET + 12);
static const uint8_t BOOST_ID = (DAIKIN_STATE_FRAME_OFFSET + 13);
// ? = 14
// ? = 15
static const uint8_t ECO_ID = (DAIKIN_STATE_FRAME_OFFSET + 16);

class DaikinClimate : public climate_ir::ClimateIR {
 public:
  DaikinClimate()
      : climate_ir::ClimateIR(this->temperature_min_(), this->temperature_max_(), 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH, climate::CLIMATE_FAN_FOCUS},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL,
                               climate::CLIMATE_SWING_HORIZONTAL, climate::CLIMATE_SWING_BOTH}) {}

  void set_swing_horizontal(bool state) { this->traits_swing_horizontal_ = state; }
  void set_swing_both(bool state) { this->traits_swing_both_ = state; }
  void set_preset_eco(bool state) { this->traits_preset_eco_ = state; }
  void set_preset_boost(bool state) { this->traits_preset_boost_ = state; }
  bool allow_preset(climate::ClimatePreset preset) const;

  void set_model(Model model) { this->model_ = model; }

 protected:
  /// Override control to change settings of the climate device.
  void control(const climate::ClimateCall &call) override;
  // Transmit via IR the state of this climate controller.
  void transmit_state() override;
  /// Return the traits of this controller.
  climate::ClimateTraits traits() override;
  uint8_t operation_mode_();
  uint16_t fan_speed_();
  uint8_t temperature_();
  // Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;
  bool parse_frame_type_(uint8_t frame_type, uint8_t frame_size, const uint8_t frame[]);
  void parse_first_frame_(const uint8_t frame[]);
  void parse_time_frame_(const uint8_t frame[]);
  void parse_state_frame_(const uint8_t frame[]);
  uint8_t frame_size_(uint8_t frame_type);

  bool traits_swing_horizontal_{false};
  bool traits_swing_both_{false};
  bool traits_preset_eco_{false};
  bool traits_preset_boost_{false};

  void preset_boost_timeout_();
  uint8_t temperature_min_();
  uint8_t temperature_max_();
  Model model_;
};

}  // namespace daikin
}  // namespace esphome
