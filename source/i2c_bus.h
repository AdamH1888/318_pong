#ifndef I2C_BUS_H
#define I2C_BUS_H

#include <stdint.h>
#include <stdbool.h>
#include "fsl_lpi2c.h"
#include "fsl_clock.h"

#define I2C_SHARED              (LPI2C2)
#define I2C_SHARED_CLOCK_HZ     CLOCK_GetLPFlexCommClkFreq(2u)

#define LCD_PCF8574_ADDR        (0x27u)
#define OLED_SH1106_ADDR        (0x3Cu)

void i2c_bus_init(void);

#endif
