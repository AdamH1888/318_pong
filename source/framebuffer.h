#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>

#define FB_WIDTH   128
#define FB_HEIGHT  64
#define FB_SIZE    (FB_WIDTH * (FB_HEIGHT / 8))

void fb_clear(uint8_t value);
void fb_set_pixel(int x, int y);
void fb_flush_to_oled(void);

#endif /* FRAMEBUFFER_H */
