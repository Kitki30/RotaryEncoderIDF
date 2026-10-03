# RotaryEncoderIDF library

Fork of mathertel's RotaryEncoder library to work with ESP-IDF

Made for IDF v6.1, not sure if it works with versions below that.

## API Guide
API Varies a lot from the original, the driver does everything without you refreshing it.
It also sets up the interrupts for you, just make sure you installed the isr handler before with ``gpio_isr_handler_add()``
No hardware init contructor is removed, other functions like ``tick()`` are not recommended to be used externally.
Some APIs may be broken, just let me known in an issue so i can fix it

Usage example:
```cpp
#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "RotaryEncoder.h"

void app_main() {
    // Install ESP32 interrupt handler
    if (gpio_install_isr_service(0) != ESP_OK) {
        printf("Failed to install GPIO ISR service!");
        abort();
    }
    
    /*
    * Latch modes available are:
    * FOUR3 - 4 steps, Latch at position 3 only (compatible to older versions)
    * FOUR0 - 4 steps, Latch at position 0 (reverse wirings)
    * TWO03 - 2 steps, Latch at position 0 and 3
    * Test them with your device and see whats the best one
    */
    RotaryEncoder encoder(1, 2, RotaryEncoder::LatchMode::TWO03); // 1 and 2 are your pin nums

    while (true) {
        /*
        * Possible directions are:
        * NOROTATION/0 - Nothing changed
        * CLOCKWISE/1 - Clockwise movement
        * COUNTERCLOCKWISE/2 - Counter-clockwise movement
        */
        printf("Current encoder direction: %d", encoder.getDirection());

        /*
        * Get current rotary enc position
        */
        printf("Current encoder position: %d", encoder.getPosition());
    
        /*
        * Get time (in milliseconds) between encoder state changes
        */
        printf("Time between change: %u", encoder.getMillisBetweenRotations());

        /*
        * Get rotations per minute
        */
        printf("RPM: %u", encoder.getRPM());
    
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```