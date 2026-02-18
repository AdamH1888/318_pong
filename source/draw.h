#ifndef DRAW_H
#define DRAW_H

#include <stdint.h>

void draw_top_border(void);
void draw_bottom_border(void);
void draw_side_borders(void);

void draw_paddle(uint8_t x, int topY, uint8_t h);
void draw_ball(int x, int y);

#endif /* DRAW_H */
