#include "draw.h"
#include "framebuffer.h"
#include "game_config.h"

static int clampValueToRange(int value, int minimumAllowed, int maximumAllowed)
{
    if (value < minimumAllowed) return minimumAllowed;
    if (value > maximumAllowed) return maximumAllowed;
    return value;
}

//Loops through each x position from left to right of the top border
//Sets the pixel at the top row (y = 0) for each x position
void draw_top_border(void)
{
    for (uint8_t x = 0; x < FB_WIDTH; x++)
    {
        fb_set_pixel((int)x, 0);
    }
}

//Loops through each x position from left to right of the bottom border
//Sets the pixel at the bottom row for each x position
void draw_bottom_border(void)
{
    for (uint8_t x = 0; x < FB_WIDTH; x++)
    {
        fb_set_pixel((int)x, FB_HEIGHT - 1);
    }
}

//Loop through each y position from top to bottom for the left and right side
//Set the pixel at the left side (x = 0) for each y position
//Set the pixel at the right side (x = FB_WIDTH - 1) for each y position
void draw_side_borders(void)
{
    for (int y = 0; y < FB_HEIGHT; y++)
    {
        fb_set_pixel(0, y);
        fb_set_pixel(FB_WIDTH - 1, y);
    }
}

//
void draw_paddle(uint8_t x, int topY, uint8_t h)
{
    int paddleHeight = (int)h; //Casts height of paddle as an integer, need correct type for calculations

    int minTop = 1;									//Defines highest position on screen so that the paddle stays within bounds
    int maxTop = (FB_HEIGHT - 2) - paddleHeight;	//Defines lowest position on screen so it fits within area as well
    topY = clampValueToRange(topY, minTop, maxTop);	//Ensures paddle drawn within screen bounds

    for (int dy = 0; dy < paddleHeight; dy++)	//Loop iterates from 0 to the height of the paddle
    {
        fb_set_pixel((int)x, topY + dy);	//For each value of 'dy' set the pixel (draw the paddle height)
    }
}

void draw_ball(int x, int y)
{

	// Set pixels for the 2x2 ball at the given (x, y) positions
	//
    fb_set_pixel(x,     y);		//Top Left
    fb_set_pixel(x + 1, y);		//Top Right
    fb_set_pixel(x,     y + 1);	//Bottom Left
    fb_set_pixel(x + 1, y + 1);	//Bottom Right
}
