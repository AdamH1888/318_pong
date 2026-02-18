#ifndef OLED_H_
#define OLED_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "MCXN947_cm33_core0.h"
#include "fsl_lpi2c.h"

/*******************************************************************************
 * I2C CONFIGURATION
 ******************************************************************************/
#define EXAMPLE_I2C_MASTER_BASE      (LPI2C2_BASE)
#define LPI2C_MASTER_CLOCK_FREQUENCY CLOCK_GetLPFlexCommClkFreq(2u)
#define EXAMPLE_I2C_MASTER           ((LPI2C_Type *)EXAMPLE_I2C_MASTER_BASE)

#define I2C_OLED                    LPI2C2
#define OLED_ADDRESS                0x3CU   /* Most SH1106 modules use 0x3C */

/*******************************************************************************
 * OLED COMMAND / DATA
 ******************************************************************************/
#define OLED_COMMAND                0x00
#define OLED_DATA                   0x40

/*******************************************************************************
 * OLED DISPLAY GEOMETRY
 ******************************************************************************/
#define OLED_WIDTH                  128
#define OLED_HEIGHT                 64
#define OLED_X_OFFSET               2
#define SH1106_RAM_WIDTH 			132

/*******************************************************************************
 * API FUNCTION PROTOTYPES
 ******************************************************************************/

/* Low-level */
void sendOLED(uint8_t *buffer, uint16_t size, uint8_t CD);

/* Addressing */
void setPage(uint8_t page);
void setSeg(uint8_t seg);

/* Init / clear */
void initOLED(void);
void resetOLED(void);
void fillOLED(uint8_t data);
void fillPage(uint8_t data);
void scrollOLED(uint8_t rows);

/* Text */
void writeChar(uint8_t character, bool inverted);
void writeString(char *string, bool inverted, uint8_t seg, uint8_t page);
void printfOLED(const char *format, ...);
void printVar(char *formatting, int32_t var, bool inverted, uint8_t seg,
		uint8_t page);
void lineWrap(void);

/* Debug */
void testFont(uint8_t startChar, uint8_t endChar);

/* Helpers */
uint8_t min(uint8_t num1, uint8_t num2);
uint8_t max(uint8_t num1, uint8_t num2);

#endif /* OLED_H_ */
