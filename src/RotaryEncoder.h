// -----
// RotaryEncoder.h - Library for using rotary encoders.
// This class is implemented for use with the ESP-IDF environment.
//
// Copyright (c) by Matthias Hertel, http://www.mathertel.de
//
// This work is licensed under a BSD 3-Clause style license,
// https://www.mathertel.de/License.aspx.
//
// More information on: http://www.mathertel.de/Arduino
// -----
// 18.01.2014 created by Matthias Hertel
// 16.06.2019 pin initialization using INPUT_PULLUP
// 10.11.2020 Added the ability to obtain the encoder RPM
// 29.01.2021 Options for using rotary encoders with 2 state changes per latch.
// 06.06.2024 Implementation of tick() with passing the input values for more performant implementations.
// 21.02.2025 Documentation and Constructor without hardware initialization added.
// 02.10.2026 Forked and rewrited for ESP-IDF by Kitki30
// -----

#pragma once
#define RotaryEncoder_h

#include <cstdint>

static void intr_handler(void *params); // Internal easy interrupt handler

class RotaryEncoder {
public:
  enum class Direction {
    NOROTATION = 0,
    CLOCKWISE = 1,
    COUNTERCLOCKWISE = -1
  };

  enum class LatchMode {
    FOUR3 = 1,  // 4 steps, Latch at position 3 only (compatible to older versions)
    FOUR0 = 2,  // 4 steps, Latch at position 0 (reverse wirings)
    TWO03 = 3   // 2 steps, Latch at position 0 and 3
  };

  /**
   * @brief Constructor that initializes the RotaryEncoder with hardware pin setup.
   *
   * This constructor creates a RotaryEncoder instance with full default hardware initialization.
   * It configures the specified pins, enables internal pull-up resistors, and reads their
   * current state to establish the initial encoder position.
   *
   * @param pin1 First encoder pin (typically pin A). Use a value 0 or greater for a valid pin.
   * @param pin2 Second encoder pin (typically pin B). Use a value 0 or greater for a valid pin.
   * @param mode The latch mode defining the encoder sensitivity.
   *   See RotaryEncoder.h for details on the available modes.
   */
  RotaryEncoder(int pin1, int pin2, LatchMode mode = LatchMode::FOUR0);

  // retrieve the current position
  long getPosition();

  // simple retrieve of the direction the knob was rotated last time. 0 = No rotation, 1 = Clockwise, -1 = Counter Clockwise
  Direction getDirection();

  // adjust the current position
  void setPosition(long newPosition);

  // Returns the time in milliseconds between the current observed
  unsigned long getMillisBetweenRotations() const;

  // Returns the RPM
  unsigned long getRPM();

  // Get pin config
  void getPins(int *pin1, int *pin2);

  void tick(int sig1, int sig2); // Set GPIO values, dont use it as this lib handles it itself

private:
  RotaryEncoder(LatchMode mode); // Basic init, turned private in fork

  int _pin1, _pin2;  // Pin numbers used for the encoder.

  LatchMode _mode;  // Latch mode from initialization

  volatile int8_t _oldState;

  volatile long _position;         // Internal position (4 times _positionExt)
  volatile long _positionExt;      // External position
  volatile long _positionExtPrev;  // External position (used only for direction checking)

  unsigned long _positionExtTime;      // The time the last position change was detected.
  unsigned long long _positionExtTimePrev;  // The time the previous position change was detected.
};