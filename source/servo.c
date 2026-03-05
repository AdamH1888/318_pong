/*
 * Pulse formula:
 *   pulse_us = SERVO_MIN_PULSE_US + (angle_x10 * range / 1800)
 *   where angle_x10 = ballX * 1800 / 127
 *   With 500-2500us range:
 *     ballX=0   -> 500 us (0 deg)
 *     ballX=63  -> ~1500 us (90 deg)
 *     ballX=127 -> 2500 us (180 deg)
 */

#include "servo.h"
#include "fsl_gpio.h"
#include "fsl_common.h"   /* SDK_DelayAtLeastUs                           */
#include "fsl_clock.h"    /* CLOCK_GetFreq, kCLOCK_CoreSysClk             */
#include "clock_config.h"

/* --------------------------------------------------------------------------
 * Module-private state
 * -------------------------------------------------------------------------- */

/* Last computed angle in fixed-point x10; used by SERVO_HOLD to freeze pos  */
static uint16_t s_last_angle_x10 = 1800u;  /* Start at 180 deg (pointing down)  */

/* --------------------------------------------------------------------------
 * Private helper: convert fixed-point angle (x10) to SG90 pulse width in us
 *   pulse_us = SERVO_MIN_PULSE_US + (angle_x10 * pulse_range / 1800)
 *   Generic formula works for any MIN/MAX pulse width setting
 * -------------------------------------------------------------------------- */
static uint32_t angle_to_pulse_us(uint16_t angle_x10)
{
    uint32_t pulse_range = SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US;
    return SERVO_MIN_PULSE_US + ((uint32_t)angle_x10 * pulse_range) / 1800u;
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void Servo_Init(void)
{
    /* GPIO0 clock is already enabled by BOARD_InitPins() — do not re-enable  */
    gpio_pin_config_t servo_config = {kGPIO_DigitalOutput, 0};
    GPIO_PinInit(SERVO_GPIO, SERVO_PIN, &servo_config);
    GPIO_PinWrite(SERVO_GPIO, SERVO_PIN, 0u);

    s_last_angle_x10 = 1800u;  /* Start pointing down (180 deg) */
}

uint32_t Servo_Update(ServoState state, uint32_t timerSeconds)
{
    uint32_t cpu_freq = CLOCK_GetFreq(kCLOCK_CoreSysClk);

    /* SERVO_OFF: no pulse, keep pin low ----------------------------------- */
    if (state == SERVO_OFF)
    {
        GPIO_PinWrite(SERVO_GPIO, SERVO_PIN, 0u);
        return 0u;  /* No pulse time consumed */
    }

    /* SERVO_TRACKING or SERVO_HOLD ---------------------------------------- */

    uint16_t angle_x10;

    if (state == SERVO_TRACKING)
    {
        /* Servo oscillates ±20 degrees (±10 degrees from 90 deg center)
         * Center position: 90 degrees (900 x10)
         * Oscillation range: 80-100 degrees (800-1000 x10)
         * Behavior:
         *   45-25 sec: slow oscillation (every 10 frames) — first 20 seconds
         *   25-5 sec: medium oscillation (every 5 frames) — middle 20 seconds
         *   5-0 sec: very fast oscillation (every 1 frame) — last 5 seconds
         */
        uint32_t max_time = 45u;  /* TIMER_TOTAL_SECONDS */
        uint16_t center_angle_x10 = 900u;  /* Center at 90 degrees */
        uint16_t oscillation_range = 200u;  /* ±20 degrees = ±200 x10 */

        /* Progressive oscillation that accelerates as time runs out */
        static uint32_t tick_counter = 0u;
        tick_counter++;
        
        /* Calculate oscillation interval based on remaining time
         * 45-25 sec: tick every 10 frames (slow)
         * 25-5 sec: tick every 5 frames (medium)
         * 5-0 sec: tick every 1 frame (very fast)
         */
        uint32_t tick_interval = 10u;  /* Default: slow */
        if (timerSeconds <= 5u) {
            tick_interval = 1u;   /* Very fast oscillation in last 5 seconds */
        } else if (timerSeconds <= 25u) {
            tick_interval = 5u;   /* Medium oscillation in middle 20 seconds */
        }
        /* else: first 20 seconds stay at 10 frame intervals (slow) */
        
        /* Calculate oscillation: alternates between center + range and center - range */
        if ((tick_counter / tick_interval) % 2u) {
            angle_x10 = center_angle_x10 + oscillation_range;  /* +20 degrees */
        } else {
            angle_x10 = center_angle_x10 - oscillation_range;  /* -20 degrees */
        }

        s_last_angle_x10 = angle_x10;
    }
    else  /* SERVO_HOLD: freeze at last tracked position */
    {
        angle_x10 = s_last_angle_x10;
    }

    uint32_t pulse_us = angle_to_pulse_us(angle_x10);

    GPIO_PinWrite(SERVO_GPIO, SERVO_PIN, 1u);       /* Assert pulse HIGH      */
    SDK_DelayAtLeastUs(pulse_us, cpu_freq);
    GPIO_PinWrite(SERVO_GPIO, SERVO_PIN, 0u);       /* De-assert pulse LOW    */
    return pulse_us;  /* Tell main loop how much time was spent               */
}
