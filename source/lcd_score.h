#ifndef LCD_SCORE_H
#define LCD_SCORE_H

#include <stdint.h>

void lcd_score_init(void);
void lcd_show_score(uint16_t left, uint16_t right);

/* Row 2 timer display — call lcd_show_timer() each second during game,
 * lcd_clear_timer() on reset to blank row 2 when returning to menu.      */
void lcd_show_timer(uint32_t seconds_remaining);
void lcd_clear_timer(void);

#endif /* LCD_SCORE_H */
