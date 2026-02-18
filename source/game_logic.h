#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include <stdbool.h>

bool verticalRangesOverlap(int topA, int bottomA, int topB, int bottomB);

void resetBallAfterPoint(
    int *ballX, int *ballY,
    int *vx, int *vy,
    bool serveToRight,
    int *servePauseFrames);

#endif /* GAME_LOGIC_H */
