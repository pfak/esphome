#include "daikin.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome {
namespace daikin {

static const char *const TAG = "daikin.climate";


void DaikinClimate::transmit_state() {
  uint8_t remote_state[35] = {
    // First Frame
    0x11, 0xDA, 0x27, 0x00, 0xC5, // Header [0-4]
    0x00, // [5]
    0x00, // Comfort: 0x00 = Off, 0x10 = On [6]
    0x00, // Checksum [7]

    // Time Frame
    0x11, 0xDA, 0x27, 0x00, 0x42, // Header [8-12]
    0x49, // Current time (mins past midnight) [13]
    0x05, // Day of the week (SUN=1, MON=2, ..., SAT=7) [14]
    0xA2, // Checksum (Hardcoded) [15]

    // State Frame
    0x11, 0xDA, 0x27, 0x00, 0x00, // Header [16-20]
    0x00, // Operation Mode [21]
    0x00, // Temperature [22]
    0x00, // [23]
    0x00, // ? Fan Speed [24]
    0x00, // ? Fan Speed [25]
    0x00, // [26]
    0x00, // [27]
    0x00, // [28]
    0x00, // [29]
    0x00, // [30]
    0xC0, // [31]
    0x00, // [32]
    0x00, // [33]
    0x00 // Checksum [34]
  };

  remote_state[21] = this->operation_mode_();
  remote_state[22] = this->temperature_();
  // Vertical swing covered in fan speed.
  uint16_t fan_speed = this->fan_speed_();
  remote_state[24] = fan_speed >> 8;
  remote_state[25] = fan_speed & 0xff;
  if (this->preset == climate::CLIMATE_PRESET_COMFORT) {
    remote_state[COMFORT_ID] = 0x10;

    if (this->swing_mode == climate::CLIMATE_SWING_BOTH) {
      this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
    } else if (this->swing_mode == climate::CLIMATE_SWING_VERTICAL) {
      this->swing_mode = climate::CLIMATE_SWING_OFF;
    }
  }

  if (this->preset == climate::CLIMATE_PRESET_BOOST) {
    remote_state[BOOST_ID] = 0x01;
    this->preset_boost_timeout_();
  }
  else if (this->preset == climate::CLIMATE_PRESET_ECO) {
    remote_state[ECO_ID] |= 0x04;
  }

  if (model_ == MODEL_ARC432A14) {
    // Frame #1
    remote_state[HEADER_ID] = 0xF0;
    remote_state[MESSAGE_ID] = 0x00;
  }
  else {
    remote_state[27] = 0x06;
    remote_state[28] = 0x60;
  }

  // Calculate checksum
  for (int i = 0; i < (DAIKIN_FIRST_FRAME_SIZE - 1); i++) {
    remote_state[7] += remote_state[i];
  }

  if (this->swing_mode == climate::CLIMATE_SWING_HORIZONTAL) {
    remote_state[SWING_HORIZONTAL_ID] |= 0xF0;
  }

  // Calculate checksum
  for (int i = DAIKIN_STATE_FRAME_OFFSET; i < (DAIKIN_STATE_FRAME_OFFSET + DAIKIN_STATE_FRAME_SIZE - 1); i++) {
    remote_state[34] += remote_state[i];
  }

  auto transmit = this->transmitter_->transmit();
  auto data = transmit.get_data();
  data->set_carrier_frequency(DAIKIN_IR_FREQUENCY);

  data->mark(DAIKIN_HEADER_MARK);
  data->space(DAIKIN_HEADER_SPACE);
  for (int i = 0; i < 8; i++) {
    for (uint8_t mask = 1; mask > 0; mask <<= 1) {  // iterate through bit mask
      data->mark(DAIKIN_BIT_MARK);
      bool bit = remote_state[i] & mask;
      data->space(bit ? DAIKIN_ONE_SPACE : DAIKIN_ZERO_SPACE);
    }
  }
  data->mark(DAIKIN_BIT_MARK);
  data->space(DAIKIN_MESSAGE_SPACE);
  data->mark(DAIKIN_HEADER_MARK);
  data->space(DAIKIN_HEADER_SPACE);

  // Second frame not necessary for ARC432A14
  if (model_ != MODEL_ARC432A14) {
    for (int i = 8; i < 16; i++) {
      for (uint8_t mask = 1; mask > 0; mask <<= 1) {  // iterate through bit mask
        data->mark(DAIKIN_BIT_MARK);
        bool bit = remote_state[i] & mask;
        data->space(bit ? DAIKIN_ONE_SPACE : DAIKIN_ZERO_SPACE);
      }
    }
    data->mark(DAIKIN_BIT_MARK);
    data->space(DAIKIN_MESSAGE_SPACE);
    data->mark(DAIKIN_HEADER_MARK);
    data->space(DAIKIN_HEADER_SPACE);
  }

  for (int i = 16; i < 35; i++) {
    for (uint8_t mask = 1; mask > 0; mask <<= 1) {  // iterate through bit mask
      data->mark(DAIKIN_BIT_MARK);
      bool bit = remote_state[i] & mask;
      data->space(bit ? DAIKIN_ONE_SPACE : DAIKIN_ZERO_SPACE);
    }
  }
  data->mark(DAIKIN_BIT_MARK);
  data->space(0);

  ESP_LOGD(TAG, "sent frame: %s", hexencode(remote_state, sizeof(remote_state)).c_str());

  transmit.perform();
}

uint8_t DaikinClimate::operation_mode_() {
  uint8_t operating_mode = DAIKIN_MODE_ON;
  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      operating_mode |= DAIKIN_MODE_COOL;
      break;
    case climate::CLIMATE_MODE_DRY:
      operating_mode |= DAIKIN_MODE_DRY;
      break;
    case climate::CLIMATE_MODE_HEAT:
      operating_mode |= DAIKIN_MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
      operating_mode |= DAIKIN_MODE_AUTO;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      operating_mode |= DAIKIN_MODE_FAN;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      operating_mode = DAIKIN_MODE_OFF;
      break;
  }

  return operating_mode;
}

uint16_t DaikinClimate::fan_speed_() {
  uint16_t fan_speed;
  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_LOW:
      fan_speed = DAIKIN_FAN_1 << 8;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      fan_speed = DAIKIN_FAN_3 << 8;
      break;
    case climate::CLIMATE_FAN_HIGH:
      fan_speed = DAIKIN_FAN_5 << 8;
      break;
    case climate::CLIMATE_FAN_FOCUS:
      fan_speed = DAIKIN_FAN_SILENT;
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      fan_speed = DAIKIN_FAN_AUTO << 8;
  }

  // If swing is enabled switch first 4 bits to 1111
  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_VERTICAL:
      fan_speed |= 0x0F00;
      break;
    case climate::CLIMATE_SWING_HORIZONTAL:
      fan_speed |= 0x000F;
      break;
    case climate::CLIMATE_SWING_BOTH:
      fan_speed |= 0x0F0F;
      break;
    default:
      break;
  }
  return fan_speed;
}

uint8_t DaikinClimate::temperature_() {
  // Force special temperatures depending on the mode
  switch (this->mode) {
    case climate::CLIMATE_MODE_FAN_ONLY:
      return 0x32;
    case climate::CLIMATE_MODE_HEAT_COOL:
    case climate::CLIMATE_MODE_DRY:
      return 0xc0;
    default:
      uint8_t temperature = (uint8_t) roundf(clamp<float>(this->target_temperature, this->temperature_min_(), this->temperature_max_()));
      return temperature << 1;
  }
}

void DaikinClimate::parse_first_frame_(const uint8_t frame[]) {
  ESP_LOGD(TAG, "first frame: %s", hexencode(frame, DAIKIN_FIRST_FRAME_SIZE).c_str());

  if (frame[COMFORT_ID] == 0x10) {
    this->preset = climate::CLIMATE_PRESET_COMFORT;

    if (this->swing_mode == climate::CLIMATE_SWING_BOTH) {
      this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
    } else if (this->swing_mode == climate::CLIMATE_SWING_VERTICAL) {
      this->swing_mode = climate::CLIMATE_SWING_OFF;
    }
  }
}

void DaikinClimate::parse_time_frame_(const uint8_t frame[]) {
  ESP_LOGD(TAG, "time frame: %s", hexencode(frame, DAIKIN_TIME_FRAME_SIZE).c_str());


}


void DaikinClimate::parse_state_frame_(const uint8_t frame[]) {
  ESP_LOGD(TAG, "state frame: %s", hexencode(frame, DAIKIN_STATE_FRAME_SIZE).c_str());

  uint8_t mode = frame[5];
  if (mode & DAIKIN_MODE_ON) {
    switch (mode & 0xF0) {
      case DAIKIN_MODE_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case DAIKIN_MODE_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case DAIKIN_MODE_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        break;
      case DAIKIN_MODE_AUTO:
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
      case DAIKIN_MODE_FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
    }
  } else {
    this->mode = climate::CLIMATE_MODE_OFF;
  }
  uint8_t temperature = frame[6];
  if (!(temperature & 0xC0)) {
    this->target_temperature = temperature >> 1;
  }
  uint8_t fan_mode = frame[8];
  uint8_t swing_mode = frame[9];
  if (fan_mode & 0xF && swing_mode & 0xF)
    this->swing_mode = climate::CLIMATE_SWING_BOTH;
  else if (fan_mode & 0xF)
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  else if (swing_mode & 0xF)
    this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
  else
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  switch (fan_mode & 0xF0) {
    case DAIKIN_FAN_1:
    case DAIKIN_FAN_2:
    case DAIKIN_FAN_SILENT:
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case DAIKIN_FAN_3:
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case DAIKIN_FAN_4:
    case DAIKIN_FAN_5:
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    case DAIKIN_FAN_AUTO:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
  }

  // should we see if the feature is set?
  uint8_t boost_mode = frame[13];
  uint8_t preset_mode = frame[16];

  if (boost_mode == 0x01) {
    this->preset = climate::CLIMATE_PRESET_BOOST;
    this->preset_boost_timeout_();
  } else if (preset_mode == 0x84) {
    this->preset = climate::CLIMATE_PRESET_ECO;
  }
}

void DaikinClimate::preset_boost_timeout_() {
    this->set_timeout("preset-boost-timeout", DAIKIN_BOOST_TIMEOUT, [this]() {
      if (this->preset == climate::CLIMATE_PRESET_BOOST)
        this->preset = climate::CLIMATE_PRESET_NONE;
        this->publish_state();
    });
}

bool DaikinClimate::on_receive(remote_base::RemoteReceiveData data) {

  uint8_t frame[DAIKIN_MAX_FRAME_SIZE] = {};
  uint8_t frame_size = DAIKIN_MAX_FRAME_SIZE, frame_type = 0x00;

  if (!data.expect_item(DAIKIN_HEADER_MARK, DAIKIN_HEADER_SPACE)) {
    return false;
  }

  for (uint8_t pos = 0; pos < DAIKIN_MAX_FRAME_SIZE; pos++) {
    uint8_t byte = 0;
    for (int8_t bit = 0; bit < 8; bit++) {
      if (data.expect_item(DAIKIN_BIT_MARK, DAIKIN_ONE_SPACE))
        byte |= 1 << bit;
      else if (!data.expect_item(DAIKIN_BIT_MARK, DAIKIN_ZERO_SPACE)) {
        return false;
      }
    }
    frame[pos] = byte;
    if (pos == 0) {
      // frame header
      if (byte != 0x11)
        return false;
    } else if (pos == 1) {
      // frame header
      if (byte != 0xDA)
        return false;
    } else if (pos == 2) {
      // frame header
      if (byte != 0x27)
        return false;
    } else if (pos == 3) {
      // frame header
      if (byte != 0x00)
        return false;
    } else if (pos == 4) {
        if ((frame_size = this->frame_size_(byte)) < 0)
          return false;

        frame_type = byte;
    }

    if (pos == (frame_size - 1))
      return this->parse_frame_type_(frame_type, frame_size, frame);
  }

  return false;
}

uint8_t DaikinClimate::frame_size_(uint8_t frame_type)
{
  switch(frame_type) {
    case DAIKIN_FIRST_FRAME_HEADER:
      return DAIKIN_FIRST_FRAME_SIZE;
    case DAIKIN_TIME_FRAME_HEADER:
      return DAIKIN_TIME_FRAME_SIZE;
    case DAIKIN_STATE_FRAME_HEADER:
      return DAIKIN_STATE_FRAME_SIZE;
    default:
      return -1;
  }
}
bool DaikinClimate::parse_frame_type_(uint8_t frame_type, uint8_t frame_size, const uint8_t frame[]) {
  ESP_LOGD(TAG, "frame type: %d, frame size: %d", frame_type, frame_size);
  uint8_t checksum = 0;
  for (int i = 0; i < (frame_size - 1); i++) {
    checksum += frame[i];
  }
  if (frame[frame_size - 1] != checksum)
      return false;

  switch(frame_type) {
    case DAIKIN_FIRST_FRAME_HEADER:
      this->parse_first_frame_(frame);
      break;
    case DAIKIN_TIME_FRAME_HEADER:
      this->parse_time_frame_(frame);
      break;
    case DAIKIN_STATE_FRAME_HEADER:
      this->parse_state_frame_(frame);
      break;
    default:
      return false;
  }

  this->publish_state();
  return true;
}

void DaikinClimate::control(const climate::ClimateCall &call) { 
  if (call.get_preset().has_value() && this->allow_preset(call.get_preset().value()) &&
      (!this->preset.has_value() || this->preset.value() != call.get_preset().value())) {
    this->preset = *call.get_preset();
  }

  ClimateIR::control(call);
}

bool DaikinClimate::allow_preset(climate::ClimatePreset preset) const {
  switch (preset) {
    case climate::CLIMATE_PRESET_ECO:
      if (this->mode != climate::CLIMATE_MODE_FAN_ONLY) {
        return true;
      } else {
        ESP_LOGD(TAG, "ECO preset is not available in FAN_ONLY mode");
      }
      break;
    case climate::CLIMATE_PRESET_NONE:
      return true;
    case climate::CLIMATE_PRESET_BOOST:
      if (traits_preset_boost_)
        return true;
    default:
      break;
  }
  return false;
}

uint8_t DaikinClimate::temperature_min_() {
  return (model_ == MODEL_ARC470A1) ? DAIKIN_ARC470A1_TEMP_MIN : DAIKIN_ARC432A14_TEMP_MIN;
}
uint8_t DaikinClimate::temperature_max_() {
  return (model_ == MODEL_ARC470A1) ? DAIKIN_ARC470A1_TEMP_MAX : DAIKIN_ARC432A14_TEMP_MAX;
}

climate::ClimateTraits DaikinClimate::traits() {
  auto traits = climate::ClimateTraits();

  traits.set_visual_min_temperature(this->temperature_min_());
  traits.set_visual_max_temperature(this->temperature_max_());
  traits.set_visual_temperature_step(1.0);
  traits.set_supports_current_temperature(true);
  traits.set_supported_modes({
      climate::CLIMATE_MODE_OFF,
      climate::CLIMATE_MODE_HEAT_COOL,
      climate::CLIMATE_MODE_COOL,
      climate::CLIMATE_MODE_DRY,
      climate::CLIMATE_MODE_HEAT,
      climate::CLIMATE_MODE_FAN_ONLY,
  });
  traits.set_supported_fan_modes({
      climate::CLIMATE_FAN_AUTO,
      climate::CLIMATE_FAN_LOW,
      climate::CLIMATE_FAN_MEDIUM,
      climate::CLIMATE_FAN_HIGH,
      climate::CLIMATE_FAN_FOCUS,
  });
  traits.set_supported_swing_modes({
      climate::CLIMATE_SWING_OFF,
      climate::CLIMATE_SWING_VERTICAL,
  });
  if (traits_swing_horizontal_)
    traits.add_supported_swing_mode(climate::CLIMATE_SWING_HORIZONTAL);
  if (traits_swing_both_)
    traits.add_supported_swing_mode(climate::CLIMATE_SWING_BOTH);
  traits.add_supported_preset(climate::CLIMATE_PRESET_NONE);
  traits.add_supported_preset(climate::CLIMATE_PRESET_COMFORT);
  if (traits_preset_eco_)
    traits.add_supported_preset(climate::CLIMATE_PRESET_ECO);
  if (traits_preset_boost_)
    traits.add_supported_preset(climate::CLIMATE_PRESET_BOOST);
  return traits;
}


}  // namespace daikin
}  // namespace esphome
