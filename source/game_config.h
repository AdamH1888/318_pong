#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

#include <stdint.h>

/* ---- Screen / paddle layout ---- */
#define BORDER_LEFT_X           0u
#define BORDER_RIGHT_X          127u

#define PADDLE_GAP              5u
#define PADDLE_H                18u

#define BALL_SPAWN_LEFT_X       10
#define BALL_SPAWN_RIGHT_X      117
#define BALL_SPAWN_Y            32

/* ---- Timing / serve ---- */
#define SERVE_PAUSE_FRAMES      15
#define SERVE_INITIAL_FRAMES    25     //Frames before first auto-serve after pressing START (~1s)

#define FRAME_PERIOD_US         20000u //Target frame period = 20ms (50Hz)
#define TIMER_TOTAL_SECONDS     30u    //Game countdown duration in seconds
#define GAME_OVER_DISPLAY_MS    3000u  //Show Game Over screen before returning to menu

#define BUZZER_BEEP_FRAMES      3      //Frames to keep buzzer on per point (~60ms at 50Hz)

/* ---- Game state ---- */
typedef enum {
	STATE_MENU_MAIN,   //Main menu showing PONG title
	STATE_MODE_SELECT, //Gamemode selection screen (Distance Sensor vs Potentiometer)
	STATE_RUNNING,     //Game is live
	STATE_PAUSED,      //Ball and AI frozen, press button to resume
	STATE_GAME_OVER    //Time up, show Game Over screen briefly
} GameState;

/* ---- Game mode (input method) ---- */
typedef enum {
	MODE_DISTANCE_SENSOR, //Left paddle uses HC-SR04, right paddle uses AI
	MODE_POTENTIOMETER    //Left paddle uses potentiometer 1, right paddle uses potentiometer 2
} GameMode;

static inline uint8_t game_left_paddle_x(void)  { return (uint8_t)(BORDER_LEFT_X + PADDLE_GAP); } //Returns X position of left paddle
static inline uint8_t game_right_paddle_x(void) { return (uint8_t)(BORDER_RIGHT_X - PADDLE_GAP); } //Returns X position of right paddle

#endif /* GAME_CONFIG_H */
