// -----
// RotaryEncoder.cpp - Library for using rotary encoders.
// This class is implemented for use with the ESP-IDF environment.
//
// Copyright (c) by Matthias Hertel, http://www.mathertel.de
// Copyright (c) by Kitki30, https://www.kitki30.com
//
// This work is licensed under a BSD 3-Clause style license,
// https://www.mathertel.de/License.aspx.
//
// More information on: http://www.mathertel.de/Arduino
// -----
// Changelog: see RotaryEncoder.h
// -----

// TODO: Add better error handling
// TODO: Refactor comments "my way", current ones look like AI (which it probably is made by)

#include "RotaryEncoder.h"

#include <cstdint>
#include <algorithm>
#include "esp_timer.h"
#include "driver/gpio.h"

#define LATCH0 0  // input state at position 0
#define LATCH3 3  // input state at position 3


// The array holds the values �1 for the entries where a position was decremented,
// a 1 for the entries where the position was incremented
// and 0 in all the other (no change or not valid) cases.

const int8_t KNOBDIR[] = {
  0, -1, 1, 0,
  1, 0, 0, -1,
  -1, 0, 0, 1,
  0, 1, -1, 0
};


// positions: [3] 1 0 2 [3] 1 0 2 [3]
// [3] is the positions where my rotary switch detends
// ==> right, count up
// <== left,  count down


// Easy interrupt handler to integrate with old API
static void intr_handler(void *params) {
  // Get RotaryEncoder class from user params
  RotaryEncoder *encoder = (RotaryEncoder*)params;

  int pin1;
  int pin2;
  encoder->getPins(&pin1, &pin2);

  // Read all two pins for less code,
  // doesnt impact performance too much
  encoder->tick(
    gpio_get_level((gpio_num_t)pin1),
    gpio_get_level((gpio_num_t)pin2)
  );
}

// ----- Initialization and Default Values -----

// Basic init, turned private in the fork
RotaryEncoder::RotaryEncoder(LatchMode mode) {
  _mode = mode;

  // No Hardware specific setup here.
  // use the ...
  _pin1 = _pin2 = -1;

  // start with position 0;
  _position = 0;
  _oldState = 0;
  _positionExtPrev = 0;
  _positionExt = 0;
  _positionExtTimePrev = esp_timer_get_time();
  _positionExtTime = _positionExtTimePrev;
} 

/**
 * @brief Constructor that initializes the RotaryEncoder with hardware pin setup.
 *
 * This constructor creates a RotaryEncoder instance with full default hardware initialization.
 * It configures the specified pins, enables internal pull-up resistors, and reads their
 * current state to establish the initial encoder position.
 *
 * @param pin1 First encoder pin (typically pin A). Use a value 0 or greater for a valid pin.
 *             A negative value or NO_PIN will skip hardware configuration.
 * @param pin2 Second encoder pin (typically pin B). Use a value 0 or greater for a valid pin.
 *             A negative value or NO_PIN will skip hardware configuration.
 * @param mode The latch mode defining the encoder sensitivity.
 *   See RotaryEncoder.h for details on the available modes.
 *
 * Hardware Setup:
 * - Configures both pins with internal pull-up resistors for reliable signal detection
 * - Reads the initial state of pin1 and pin2 using gpio_get_level()
 * - Establishes the initial position based on current pin values
 * - Stores pin numbers for use with the non-parameterized tick() method
 * - Sets up interrupts for it to be faster and non-blocking
 *
 * Initial State:
 * - Position counter initialized to 0
 * - Internal state variables set based on reading the actual pin values
 * - Timestamp tracking initialized for rotation speed calculation
 *
 * @note If both pins are negative or not in the valid range [0, MAX_PIN], the hardware
 *       setup is skipped but the encoder still initializes with software defaults.
 */
RotaryEncoder::RotaryEncoder(int pin1, int pin2, LatchMode mode) : RotaryEncoder(mode) {
  int sig1 = 0;
  int sig2 = 0;

  // Remember Hardware Setup
  _pin1 = pin1;
  _pin2 = pin2;

  // Setup the input pins and turn on pullup resistor
  gpio_config_t gpio_cfg = {
    .pin_bit_mask = (1ULL << pin1 | 1ULL << pin2),
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_ENABLE,
    .intr_type = GPIO_INTR_ANYEDGE
  };

  gpio_config(&gpio_cfg);

  // when not started in motion, the current state of the encoder should be 3
  sig1 = gpio_get_level((gpio_num_t)_pin1);
  sig2 = gpio_get_level((gpio_num_t)_pin2);

  // Attach interrupts
  gpio_isr_handler_add((gpio_num_t)_pin1, intr_handler, (void*)this);
  gpio_isr_handler_add((gpio_num_t)_pin2, intr_handler, (void*)this);

  _oldState = sig1 | (sig2 << 1);
}  // RotaryEncoder()

RotaryEncoder::Direction RotaryEncoder::getDirection() {
  RotaryEncoder::Direction ret = Direction::NOROTATION;

  if (_positionExtPrev > _positionExt) {
    ret = Direction::COUNTERCLOCKWISE;
    _positionExtPrev = _positionExt;
  } else if (_positionExtPrev < _positionExt) {
    ret = Direction::CLOCKWISE;
    _positionExtPrev = _positionExt;
  } else {
    ret = Direction::NOROTATION;
    _positionExtPrev = _positionExt;
  }

  return ret;
}

long RotaryEncoder::getPosition() {
  return _positionExt;
} 

void RotaryEncoder::setPosition(long newPosition) {
  switch (_mode) {
    case LatchMode::FOUR3:
    case LatchMode::FOUR0:
      // only adjust the external part of the position.
      _position = ((newPosition << 2) | (_position & 0x03L));
      _positionExt = newPosition;
      _positionExtPrev = newPosition;
      break;

    case LatchMode::TWO03:
      // only adjust the external part of the position.
      _position = ((newPosition << 1) | (_position & 0x01L));
      _positionExt = newPosition;
      _positionExtPrev = newPosition;
      break;
  }  // switch

}  // setPosition()



// Updates signals to new values
void RotaryEncoder::tick(int sig1, int sig2) {
  unsigned long now = esp_timer_get_time();
  int8_t thisState = sig1 | (sig2 << 1);

  if (_oldState != thisState) {
    _position += KNOBDIR[thisState | (_oldState << 2)];
    _oldState = thisState;

    switch (_mode) {
      case LatchMode::FOUR3:
        if (thisState == LATCH3) {
          // The hardware has 4 steps with a latch on the input state 3
          _positionExt = _position >> 2;
          _positionExtTimePrev = _positionExtTime;
          _positionExtTime = now;
        }
        break;

      case LatchMode::FOUR0:
        if (thisState == LATCH0) {
          // The hardware has 4 steps with a latch on the input state 0
          _positionExt = _position >> 2;
          _positionExtTimePrev = _positionExtTime;
          _positionExtTime = now;
        }
        break;

      case LatchMode::TWO03:
        if ((thisState == LATCH0) || (thisState == LATCH3)) {
          // The hardware has 2 steps with a latch on the input state 0 and 3
          _positionExt = _position >> 1;
          _positionExtTimePrev = _positionExtTime;
          _positionExtTime = now;
        }
        break;
    }  // switch
  }  // if
}  // tick()


unsigned long RotaryEncoder::getMillisBetweenRotations() const {
  return (_positionExtTime - _positionExtTimePrev);
}

unsigned long RotaryEncoder::getRPM() {
  // calculate max of difference in time between last position changes or last change and now.
  unsigned long timeBetweenLastPositions = _positionExtTime - _positionExtTimePrev;
  unsigned long timeToLastPosition = esp_timer_get_time() - _positionExtTime;
  unsigned long t = std::max(timeBetweenLastPositions, timeToLastPosition);
  return 60000.0 / ((float)(t * 20));
}

// Get pins that RotaryEncoder is initialized with in an array
// Use this if you want your own interrupts
void RotaryEncoder::getPins(int *pin1, int *pin2) {
  *pin1 = _pin1;
  *pin2 = _pin2;
}

// End