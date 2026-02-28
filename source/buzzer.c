#include "buzzer.h"
#include "fsl_gpio.h"
#include <stdbool.h>
#include <stdint.h>

#define BUZZER_GPIO          GPIO0
#define BUZZER_PIN           31u

/* 1 = high level active; 0 = low level active */
#define BUZZER_ACTIVE_LEVEL  1u

static uint32_t s_buzzerFramesRemaining = 0u;

static void Buzzer_Write(bool on)
{
    uint8_t level;

    if (on) {
        level = (uint8_t)BUZZER_ACTIVE_LEVEL;
    } else {
        level = (uint8_t)(BUZZER_ACTIVE_LEVEL ? 0u : 1u);
    }

    GPIO_PinWrite(BUZZER_GPIO, BUZZER_PIN, level);
}

void Buzzer_Init(void)
{
    gpio_pin_config_t buzzerConfig = {
        kGPIO_DigitalOutput,
        (uint8_t)(BUZZER_ACTIVE_LEVEL ? 0u : 1u)   /* Default off */
    };

    GPIO_PinInit(BUZZER_GPIO, BUZZER_PIN, &buzzerConfig);

    s_buzzerFramesRemaining = 0u;
    Buzzer_Off();
}

void Buzzer_On(void)
{
    Buzzer_Write(true);
}

void Buzzer_Off(void)
{
    Buzzer_Write(false);
}

void Buzzer_Stop(void)
{
    s_buzzerFramesRemaining = 0u;
    Buzzer_Off();
}

void Buzzer_Beep(uint32_t frames)
{
    if (frames == 0u) {
        Buzzer_Stop();
        return;
    }

    s_buzzerFramesRemaining = frames;
    Buzzer_On();
}

void Buzzer_Update(void)
{
    if (s_buzzerFramesRemaining > 0u) {
        s_buzzerFramesRemaining--;

        if (s_buzzerFramesRemaining == 0u) {
            Buzzer_Off();
        }
    }
}
