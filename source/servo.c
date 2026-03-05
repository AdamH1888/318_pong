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
        /* Map timer countdown to servo position (180 deg down -> 0 deg up as time counts down)
         * timerSeconds: 30 -> 180°, 0 -> 0°
         * Formula: angle = timerSeconds / 30 * 180
         */
        uint32_t max_time = 30u;  /* TIMER_TOTAL_SECONDS */

        /* Base sweep: servo points down at start, sweeps up as time counts down */
        if (timerSeconds > max_time) timerSeconds = max_time;
        uint16_t base_angle_x10 = (uint16_t)((timerSeconds * 1800u) / max_time);

        /* Progressive ticking that accelerates as time runs out */
        static uint32_t tick_counter = 0u;
        tick_counter++;
        
        /* Calculate tick interval based on remaining time
         * 30-20 sec: tick every 10 frames (slow)
         * 20-10 sec: tick every 5 frames (medium)
         * 10-0 sec: tick every 1 frame (very fast)
         */
        uint32_t tick_interval = 10u;  /* Default: slow */
        if (timerSeconds < 10u) {
            tick_interval = 1u;  /* Fast tick in last 10 seconds */
        } else if (timerSeconds < 20u) {
            tick_interval = 5u;  /* Medium tick */
        }
        
        /* Only apply ticking after first few seconds (when sweep has progressed) */
        if (timerSeconds < 28u) {
            uint16_t tick_offset = ((tick_counter / tick_interval) % 2u) ? 150u : 0u;  /* ±15 deg oscillation */
            
            if (base_angle_x10 >= tick_offset) {
                angle_x10 = base_angle_x10 - tick_offset;
            } else {
                angle_x10 = base_angle_x10 + tick_offset;
            }
        } else {
            angle_x10 = base_angle_x10;  /* No ticking at very start */
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
