/* lcd.h
 * SPDX-License-Identifier: MIT-0
 *
 * Source / provenance:
 *   This LCD1602 driver targets HD44780-compatible character LCDs connected via a
 *   PCF8574/PCF8574A I2C “backpack”. The implementation follows the standard,
 *   widely-documented HD44780 4-bit initialization + transfer sequence, with the
 *   PCF8574 used as a remote 8-bit GPIO expander to drive RS/EN/D4..D7 (RW kept low).
 * Licensing intent:
 *   You may use/modify/redistribute this file content under MIT-0 (a permissive,
 *   attribution-light license). If your project has a preferred license file/
 *   NOTICE format, you can copy this header text into your project’s NOTICE.
 */

#ifndef PCF8574_H
#define PCF8574_H

#include <stdint.h>
#include <stdbool.h>
#include "fsl_lpi2c.h"
#include "fsl_common.h"
#include "fsl_clock.h"
#include "i2c_bus.h"

#ifndef PCF8574_I2C_BASE
#define PCF8574_I2C_BASE (I2C_SHARED)
#endif

#ifndef PCF8574_I2C_ADDRESS_7BIT
#define PCF8574_I2C_ADDRESS_7BIT (LCD_PCF8574_ADDR)
#endif

#ifndef PCF8574_DO_I2C_INIT
#define PCF8574_DO_I2C_INIT (0u)
#endif

void pcf8574_send_cmd(uint8_t cmd);				//Sends a command to the LCD, splits 8-bit byte into two 4 bit nibbles
void pcf8574_send_data(uint8_t data);			//Sends data such as characters to the LCD, also uses nibbles
void pcf8574_cursor(uint8_t row, uint8_t col);	//Moves the cursor to the specified row and column on the LCD
void pcf8574_send_string(char *str);			//Sends each character of the string by calling 'send data' function for each character
void pcf8574_init(void);						//Clears the screen and prepares it to display text
void pcf8574_set_backlight(bool on);			//Turns the back light on or off

#endif /* PCF8574_H */
