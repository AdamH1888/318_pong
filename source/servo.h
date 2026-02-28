#ifndef SERVO_H
#define SERVO_H

/*
 * servo.h — SG90 servo driver for FRDM-MCXN947 Pong game
 *
 * Tracks the ball's horizontal position in real time:
 *   Ball at left edge  (X=0)   -> servo at 0 deg
 *   Ball at centre     (X=63)  -> servo at 90 deg
 *   Ball at right edge (X=127) -> servo at 180 deg
 *
 * Signal wire connects to GPIO0_28 (pin E8 / J2[2]).
 * GPIO0 clock and PORT0_28 mux are configured by BOARD_InitPins() in pin_mux.c.
 *
 * Each call to Servo_Tick() blocks for exactly SERVO_PERIOD_US (20 ms),
 * replacing the bare SDK_DelayAtLeastUs(20000u, ...) at the end of the main loop.
 */

#include <stdint.h>
#include <stdbool.h>
#include "fsl_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 * Hardware constants
 * -------------------------------------------------------------------------- */

#define SERVO_GPIO          GPIO0
#define SERVO_PIN           28u

/* SG90 PWM timing in microseconds */
#define SERVO_PERIOD_US     20000u  /* Full 20 ms PWM period    */
#define SERVO_MIN_PULSE_US   1000u  /* 1000 us = 0 degrees      */
#define SERVO_MAX_PULSE_US   2000u  /* 2000 us = 180 degrees    */

/* Ball X range on the 128-pixel-wide screen */
#define SERVO_BALL_X_MAX    127u

/* --------------------------------------------------------------------------
 * State — map from GameState before each Servo_Tick() call:
 *   STATE_MENU    -> SERVO_OFF
 *   STATE_RUNNING -> SERVO_TRACKING
 *   STATE_PAUSED  -> SERVO_HOLD
 * -------------------------------------------------------------------------- */
typedef enum {
    SERVO_OFF,       /* No pulses — motor de-energizes (menu)                 */
    SERVO_TRACKING,  /* Tracks ball X position — updates angle every frame    */
    SERVO_HOLD       /* Holds last position — pulse sent at frozen angle      */
} ServoState;

/* --------------------------------------------------------------------------
 * API
 * -------------------------------------------------------------------------- */

/*
 * Servo_Init — configure GPIO0_28 as digital output driven low.
 * Call once after BOARD_InitHardware().
 */
void Servo_Init(void);

/*
 * Servo_Update — generate countdown timer visual effect via servo position.
 * Slowly sweeps servo from left (0°) to right (180°) as game timer counts down.
 * In the last 10 seconds, adds fast oscillating "ticking" to warn of time running out.
 *
 * Returns the pulse duration in microseconds (0 if SERVO_OFF), so the
 * caller can subtract it from the 20ms frame delay to keep frame timing accurate.
 *
 * timerSeconds: current game time remaining (0-30, or whatever TIMER_TOTAL_SECONDS is)
 *
 * Note: Game loop timing is now independent of servo hardware.
 *       Call this before the explicit 20ms frame delay.
 */
uint32_t Servo_Update(ServoState state, uint32_t timerSeconds);

#ifdef __cplusplus
}
#endif

#endif /* SERVO_H */
