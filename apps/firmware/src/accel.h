#ifndef ACCEL_H
#define ACCEL_H

#include <stdint.h>
#include <stdbool.h>

#define ACCEL_INT1_PIN      16
#define ACCEL_SCL_PIN       18
#define ACCEL_SDA_PIN       20

#define LIS2DH12_ADDR       0x19

typedef void (*accel_motion_handler_t)(void);

/**
 * Initializes the accelerometer via I2C and configures INT1 for motion detection.
 * The handler is called from interrupt context when motion is detected.
 */
void accel_init(accel_motion_handler_t handler);

/**
 * Puts the accelerometer into ultra-low-power mode while keeping
 * the motion interrupt active. Call after startup configuration.
 */
void accel_enter_low_power(void);

#endif
