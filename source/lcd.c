/* lcd.c
 * SPDX-License-Identifier: MIT-0
 *
 * Source / provenance:
 *   - Implements the HD44780 “4-bit mode” write protocol using a PCF8574/PCF8574A
 *     I2C I/O expander (typical LCD1602 I2C backpack).
 *   - Transfer pattern is the standard approach: for each byte, send the upper
 *     nibble then lower nibble, and “pulse” EN high->low to latch each nibble.
 *   - Bit mapping assumed (common backpack wiring):
 *       P0=RS, P1=RW, P2=EN, P3=Backlight, P4=D4, P5=D5, P6=D6, P7=D7
 * Licensing intent:
 *   MIT-0 (permissive). Keep this header in redistributed source form.
 */

#include "lcd.h"
#include <string.h>

#define LCD_RS (1u << 0)
#define LCD_RW (1u << 1)
#define LCD_EN (1u << 2)
#define LCD_BL (1u << 3)

static bool s_backlight_on = true;
static uint8_t s_shadow = 0u;

static inline uint8_t bl_mask(void)
{
    return s_backlight_on ? (uint8_t)LCD_BL : 0u;
}

static inline void delay_us(uint32_t us)
{
    SDK_DelayAtLeastUs(us, CLOCK_GetFreq(kCLOCK_CoreSysClk));
}

static inline void delay_ms(uint32_t ms)
{
    while (ms--) delay_us(1000u);
}

static status_t i2c_write_buf(const uint8_t *buf, size_t len)
{
    lpi2c_master_transfer_t xfer;
    memset(&xfer, 0, sizeof(xfer));
    xfer.slaveAddress = PCF8574_I2C_ADDRESS_7BIT; /* 7-bit */
    xfer.direction    = kLPI2C_Write;
    xfer.data         = (uint8_t *)buf;
    xfer.dataSize     = (uint16_t)len;
    xfer.flags        = kLPI2C_TransferDefaultFlag;
    return LPI2C_MasterTransferBlocking(PCF8574_I2C_BASE, &xfer);
}

static void send4bytes(uint8_t du, uint8_t dl, uint8_t low_nibble_flags)
{
    /* low_nibble_flags must already include RS if needed. RW stays 0. */
    uint8_t bl = bl_mask();

    uint8_t data[4];
    data[0] = (uint8_t)(du | (low_nibble_flags | LCD_EN) | bl);
    data[1] = (uint8_t)(du | (low_nibble_flags)          | bl);
    data[2] = (uint8_t)(dl | (low_nibble_flags | LCD_EN) | bl);
    data[3] = (uint8_t)(dl | (low_nibble_flags)          | bl);

    s_shadow = (uint8_t)(data[3] & (uint8_t)~LCD_BL);

    (void)i2c_write_buf(data, sizeof(data));
    delay_us(50);
}

void pcf8574_send_cmd(uint8_t cmd)
{
    uint8_t du = (uint8_t)(cmd & 0xF0u);
    uint8_t dl = (uint8_t)((cmd << 4) & 0xF0u);
    send4bytes(du, dl, 0u); /* RS=0 */
}

void pcf8574_send_data(uint8_t data)
{
    uint8_t du = (uint8_t)(data & 0xF0u);
    uint8_t dl = (uint8_t)((data << 4) & 0xF0u);
    send4bytes(du, dl, LCD_RS); /* RS=1 */
}

void pcf8574_set_backlight(bool on)
{
    s_backlight_on = on;
    uint8_t out = (uint8_t)(s_shadow | bl_mask());
    (void)i2c_write_buf(&out, 1u);
}

void pcf8574_cursor(uint8_t row, uint8_t col)
{
    uint8_t addr = (row == 0u) ? (uint8_t)(0x00u + col) : (uint8_t)(0x40u + col);
    pcf8574_send_cmd((uint8_t)(0x80u | addr));
}

void pcf8574_send_string(char *str)
{
    while (str && *str)
        pcf8574_send_data((uint8_t)*str++);
}

void pcf8574_init(void)
{
#if (PCF8574_DO_I2C_INIT != 0u)

#endif

    delay_ms(50);

    s_shadow = 0u;
    (void)i2c_write_buf((uint8_t[]){ (uint8_t)(s_shadow | bl_mask()) }, 1u);

    for (uint8_t i = 0; i < 3; i++)
    {
        pcf8574_send_cmd(0x03);
        delay_ms(5);
    }

    pcf8574_send_cmd(0x02); /* 4-bit mode */
    delay_ms(100);

    pcf8574_send_cmd(0x28); /* 2-line, 5x8 */
    delay_ms(1);

    pcf8574_send_cmd(0x01); /* clear */
    delay_ms(2);

    pcf8574_send_cmd(0x06); /* entry mode */
    delay_ms(1);

    pcf8574_send_cmd(0x0C); /* display on */
    delay_ms(1);
}
