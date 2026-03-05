#include "game_logic.h"
#include "game_config.h"

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

