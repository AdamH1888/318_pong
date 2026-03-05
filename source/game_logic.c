#include "game_logic.h"
#include "game_config.h"

/* ---- AI paddle constants ---- */
#define AI_REACT_EVERY_N_FRAMES  5	//AI paddle reacts every N frames
#define AI_DEADZONE_PIXELS       2	//Ball is within N pixels of AI paddle center, then paddle won't move
#define AI_HESITATE_PERCENT      25	//Percentage chance that the AI will hesitate (do nothing)

/* ---- Simple RNG used by AI hesitation ---- */
static uint32_t rngState = 0x12345678u;
static uint32_t rngNext(void) {
	rngState = (1103515245u * rngState + 12345u);
	return rngState;
}

//Clamp a value so it never goes below min or above max
static int clampValueToRange(int value, int minimumAllowed, int maximumAllowed) {
	if (value < minimumAllowed)
		return minimumAllowed;
	if (value > maximumAllowed)
		return maximumAllowed;
	return value;
}

//Function checks if two vertical ranges overlap
//topA, bottomA: The top and bottom coordinates of range A
//topB, bottomB: The top and bottom coordinates of range B
//Checks if the bottom of A is above the top of B and if the bottom of B is above the top of A
//The operator inverts the result meaning if the ranges do overlap it returns true
bool verticalRangesOverlap(int topA, int bottomA, int topB, int bottomB)
{
    return !(bottomA < topB || bottomB < topA);
}

//Reset the ball to starting position after a point is scored
//The function gets the memory address of these variables and any changes made to them directly affect the original variables outside the function
void resetBallAfterPoint(
    int *ballX, int *ballY,
    int *vx, int *vy,
    bool serveToRight,
    int *servePauseFrames)
{
    if (serveToRight) //Checks whether the ball should be served to the right
    {
        *ballX = BALL_SPAWN_LEFT_X; //Left paddle serves
        *vx = 4;					//Sets ball X velocity to positive so it moves to right (4 pixels per frame to right)
    }
    else
    {
        *ballX = BALL_SPAWN_RIGHT_X; //Right paddle serves
        *vx = -4;					 //Sets ball x velocity to negative so it moves to left (4 pixels per frame to left)
    }

    *ballY = BALL_SPAWN_Y;					//Set the ball's Y position to the starting Y-coordinate (32=center)
    *vy = 2;								//Move downwards at 2 pixels per frame
    *servePauseFrames = SERVE_PAUSE_FRAMES;	//Serve pause set to specific amount of frames
}

//Move paddle up or down towards the ball while keeping paddle within screen limits
void updateAiPaddle(int *paddleTopY, int ballCenterY, int frameCount) {
	if ((frameCount % AI_REACT_EVERY_N_FRAMES) != 0)
		return;

	if ((rngNext() % 100u) < AI_HESITATE_PERCENT)
		return;

	int paddleCenterY = *paddleTopY + (PADDLE_H / 2);
	int diff = ballCenterY - paddleCenterY;

	if (diff > AI_DEADZONE_PIXELS)
		(*paddleTopY)++;
	if (diff < -AI_DEADZONE_PIXELS)
		(*paddleTopY)--;

	*paddleTopY = clampValueToRange(*paddleTopY, 1, 62 - PADDLE_H);
}

//Adjust ball Y velocity based on where it hits the paddle (3-zone approach)
//Top third: strong upward angle | Middle third: normal | Bottom third: strong downward angle
void adjustBallAngleFromPaddleHit(int *ballVelocityY, int ballCenterY, int paddleTopY) {
	const int paddleThird = PADDLE_H / 3;
	int hitOffset = ballCenterY - paddleTopY;

	if (hitOffset < paddleThird) {
		*ballVelocityY = -2;
	} else if (hitOffset >= (paddleThird * 2)) {
		*ballVelocityY = 2;
	} else {
		*ballVelocityY = 0;
	}
}

