#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include <stdbool.h>

bool verticalRangesOverlap(int topA, int bottomA, int topB, int bottomB);

void resetBallAfterPoint(
    int *ballX, int *ballY,
    int *vx, int *vy,
    bool serveToRight,
    int *servePauseFrames);

//Move the AI paddle towards the ball, skipping some frames and adding hesitation
void updateAiPaddle(int *paddleTopY, int ballCenterY, int frameCount);

//Adjust ball Y velocity based on where it hits the paddle (top/middle/bottom third)
void adjustBallAngleFromPaddleHit(int *ballVelocityY, int ballCenterY, int paddleTopY);

#endif /* GAME_LOGIC_H */
