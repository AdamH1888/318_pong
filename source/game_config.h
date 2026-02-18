#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

#include <stdint.h>

#define BORDER_LEFT_X           0u
#define BORDER_RIGHT_X          127u

#define PADDLE_GAP              5u
#define PADDLE_H                18u

#define BALL_SPAWN_LEFT_X       10
#define BALL_SPAWN_RIGHT_X      117
#define BALL_SPAWN_Y            32

#define SERVE_PAUSE_FRAMES      15

static inline uint8_t game_left_paddle_x(void)  { return (uint8_t)(BORDER_LEFT_X + PADDLE_GAP); } //Returns X position of left paddle
static inline uint8_t game_right_paddle_x(void) { return (uint8_t)(BORDER_RIGHT_X - PADDLE_GAP); } //Returns X position of right paddle

#endif /* GAME_CONFIG_H */
