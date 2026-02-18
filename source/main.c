#include "fsl_device_registers.h"
#include "fsl_clock.h"
#include "fsl_lpi2c.h"              //LPI2C driver (used for OLED and LCD)
#include "clock_config.h"
#include "board.h"
#include "app.h"
#include "fsl_debug_console.h"
#include "i2c_bus.h"                //Small helper module used by OLED and LCD (Share same bus)
#include "oled.h"                   //OLED driver
#include "lcd_score.h"              //Score display on the 16x2 character LCD
#include "distance_sensor.h"        //HC-SR04 distance sensor driver
#include "game_config.h"            //Game settings (screen size, paddle size, speeds etc)
#include "game_logic.h"             //Physics/collisions/scoring logic
#include "framebuffer.h"            //Buffer in RAM that represents OLED pixels
#include "draw.h"                   //Drawing paddles/ball/border
#include <stdint.h>
#include <stdbool.h>

#define AI_REACT_EVERY_N_FRAMES  3	//AI paddle reacts every N frames
#define AI_DEADZONE_PIXELS       2	//Ball is within N pixels of AI paddle center, then paddle won't move
#define AI_HESITATE_PERCENT      15	//Percentage chance that the AI will hesitate (do nothing)

//Function that generates a random number each time it is called
//static allows variable to hold value across multiple function calls, it won't reset each time
static uint32_t rngState = 0x12345678u;
static uint32_t rngNext(void) {
	rngState = (1103515245u * rngState + 12345u); //Standard commonly used RNG
	return rngState;
}

//Function that clamps a value so it never goes below min or above max
static int clampValueToRange(int value, int minimumAllowed, int maximumAllowed) {
	if (value < minimumAllowed)
		return minimumAllowed;
	if (value > maximumAllowed)
		return maximumAllowed;
	return value;
}

//Function to move paddle up or down towards the ball while keeping paddle within screen limits
//paddleTopY: pointer to directly modify the paddle's top edge Y position (At top of screen Y=1 down to Y=62)
//Pointer can pass address of variable to function, allowing the function to modify the original variable directly
//ballCenterY: the ball’s current Y (bottom Y pixel of ball)
//frameCount: current frame number
static void updateAiPaddle(int *paddleTopY, int ballCenterY, int frameCount) {
	if ((frameCount % AI_REACT_EVERY_N_FRAMES) != 0) //Check if frames divisible by 3, if so move to next stage, if not AI does nothing
		return;

	if ((rngNext() % 100u) < AI_HESITATE_PERCENT) //Checks remainder when dividing RNG by 100, if less than the value selected it triggers hesitation
		return;

	int paddleCenterY = *paddleTopY + (PADDLE_H / 2); //Works out paddle center Y position
	int diff = ballCenterY - paddleCenterY; //Compares it to ball center Y position ('diff' positive then ball lower on screen)

	if (diff > AI_DEADZONE_PIXELS)  //If ball below paddle center by more than deadzone
		(*paddleTopY)++; 			//Move paddle down
	if (diff < -AI_DEADZONE_PIXELS)	//If ball above paddle center by more than deadzone
		(*paddleTopY)--; 			//Move paddle above

	*paddleTopY = clampValueToRange(*paddleTopY, 1, 62 - PADDLE_H); //Clamp paddle so it stays on screen
}

//Function converts the measured hand distance from sensor (cm) into a paddle Y position (top of paddle)
//Range of distance sensor set by cmNear and cmFar
static int mapDistanceCmToPaddleY(float cm) {
	const float cmNear = 5.0f; //Closest distance that will move paddle down (pixel value highest)
	const float cmFar = 35.0f; //Farthest distance that will move paddle up (pixel value lowest)

	if (cm < cmNear) //If distance less than closest boundary
		cm = cmNear; //Clamp it to the closest boundary
	if (cm > cmFar)	 //If distance more than farthest boundary
		cm = cmFar;  //Clamp it to the farthest boundary

	float t = (cm - cmNear) / (cmFar - cmNear); //Normalised value, t=0=near, t=1=far

	//Allowed paddle Y range (Taking into account height of paddle)
	const int yMin = 1;
	const int yMax = 62 - PADDLE_H;

	//Picks a paddle pixel Y position between min and max based on distance (normalised t)
	int y = (int) ((1.0f - t) * (yMax - yMin) + yMin); //t=0, 1-0=1, 1*(yMax - yMin) + yMin = yMax (Lowest point on screen, y = 62)
	return y; //Paddle top edge pixel number
}

int main(void) {
	BOARD_InitHardware();

	i2c_bus_init();		//Initialises I2C bus shared by OLED and LCD
	initOLED();			//Initialises OLED display
	lcd_score_init();	//Initialises score system on LCD character display

	//Sensor struct showing what GPIO and pin are used for TRIG and ECHO
	hcsr04_t sensor = { .trigGpio = GPIO0, .trigPin = 29u, .echoGpio = GPIO1,
			.echoPin = 23u, };

	CLOCK_EnableClock(kCLOCK_Gpio0);
	CLOCK_EnableClock(kCLOCK_Gpio1);

	//Initialises this so readings from distance sensor can be taken
	HCSR04_Init(&sensor);

	//Initialises score on LCD as 0-0
	lcd_show_score(0, 0);

	//This is the X positions of the paddles (fixed left and right columns)
	const uint8_t leftPaddleX = game_left_paddle_x();
	const uint8_t rightPaddleX = game_right_paddle_x();

	//This is the Y positions of the top of each paddle (of course change during game)
	int leftPaddleTopY = 24;
	int rightPaddleTopY = 24;

	//Ball starting position, serve from left side at start
	int ballX = BALL_SPAWN_LEFT_X;
	int ballY = BALL_SPAWN_Y;

	//How many pixels the ball moves each frame
	int ballVelocityX = 4;
	int ballVelocityY = 2;

	int frameCount = 0;
	int servePauseFrames = 0;

	uint16_t scoreLeft = 0;
	uint16_t scoreRight = 0;

	//LCD is only updated when score changes not every frame, this ensures it updates LCD score immediately to 0-0 at the start
	//0xFFFF very safe as every bit turned on (65535), so last score never equals score
	uint16_t lastScoreLeft = 0xFFFFu;
	uint16_t lastScoreRight = 0xFFFFu;

	while (1) {
		frameCount++;

		int ballCenterY = ballY + 1; //The ball is 2 pixels tall so adding 1 gives bottom pixel ball value

		//Left paddle is controlled by distance sensor, need a sensor frame counter as reading it every frame can cause noise
		//Count frames since last sensor read
		static int sensorFrameCounter = 0;
		sensorFrameCounter++;

		if (sensorFrameCounter >= 2) //Read sensor value every 2 frames
				{
			sensorFrameCounter = 0;	//Reset counter so it counts again until next read

			float cm; //Variable to store measured distance

			//Try to read sensor, if it returns true then reading is valid and 'cm' has the distance
			if (HCSR04_ReadCm(&sensor, &cm)) {
				leftPaddleTopY = mapDistanceCmToPaddleY(cm); //Convert distance into a paddle top edge Y position on screen
				leftPaddleTopY = clampValueToRange(leftPaddleTopY, 1, 62 - PADDLE_H); //Clamp paddle so it stays in area
			}
		}

		//Update right paddle, using AI for now to control
		updateAiPaddle(&rightPaddleTopY, ballCenterY, frameCount);

		// After someone scores, you pause the serve for a few frames, value set in game logic file
		// If we’re still in the pause, count down
		// If not paused, move the ball each frame by its set velocities
		if (servePauseFrames > 0) {
			servePauseFrames--;
		} else {
			ballX += ballVelocityX;
			ballY += ballVelocityY;
		}

		//If ball bounces off the top wall push ball back inside and reverse the Y direction
		if (ballY <= 1) {
			ballY = 1;
			ballVelocityY = -ballVelocityY;
		}

		//If ball bounces of the bottom wall make it stay inside and reverse the Y direction (ball 2 pixels therefore ballY+1)
		if (ballY + 1 >= 62) {
			ballY = 61;
			ballVelocityY = -ballVelocityY;
		}

		//Handles left paddle collision logic
		//Only check the left paddle when the ball is moving left so the velocity is less than 0 (negative)
		//And also when the ball reaches the left paddle's X position
		if (ballVelocityX < 0 && ballX <= (int) leftPaddleX) {

			//Check if the ball's vertical range overlaps the paddle's vertical range.
			//Ball spans from ballY to ballY+1
			//Paddle spans from leftPaddleTopY to leftPaddleTopY + PADDLE_H - 1
			bool hitLeft = verticalRangesOverlap(ballY, ballY + 1,
					leftPaddleTopY, leftPaddleTopY + (PADDLE_H - 1));

			if (hitLeft) {
				//If the ball hits the paddle
				//Move it slightly away from paddle so it doesn't stick
				//Reverse X velocity so it goes back to the right
				ballX = (int) leftPaddleX + 1;
				ballVelocityX = -ballVelocityX;
			} else {
				//Ball misses the paddle then the right player scores a point
				scoreRight++;

				//Reset the ball for next point
				//The 'true' here means "serve to the right", this is a function from game logic file
				//Also sets servePauseFrames so the game pauses briefly after a point
				resetBallAfterPoint(&ballX, &ballY, &ballVelocityX,
						&ballVelocityY, true, &servePauseFrames);
			}
		}

		// Right paddle scoring logic
		// Only check the right paddle when the ball is moving right so the velocity must be greater than 0
		// And also when the ball has reached the right paddle's X position
		if (ballVelocityX > 0 && (ballX + 1) >= (int) rightPaddleX) {

			// Check if the ball's vertical range overlaps the paddle's vertical range.
			// ball spans from ballY to ballY+1
			// paddle spans from rightPaddleTopY to rightPaddleTopY + PADDLE_H - 1
			bool hitRight = verticalRangesOverlap(ballY, ballY + 1,
					rightPaddleTopY, rightPaddleTopY + (PADDLE_H - 1));

			if (hitRight) {
				//Ball hit the right paddle
				//Move the ball away from paddle so it doesn't get stuck
				//Reverse the X velocity so the ball goes back to the left
				ballX = (int) rightPaddleX - 2;
				ballVelocityX = -ballVelocityX;
			} else {
				////Ball misses the paddle then the left player scores a point
				scoreLeft++;

				//Reset the ball for next point
				//The 'false' here means "serve to the left", this is a function from game logic file
				//Also sets servePauseFrames so the game pauses briefly after a point
				resetBallAfterPoint(&ballX, &ballY, &ballVelocityX,
						&ballVelocityY, false, &servePauseFrames);
			}
		}

		//Check if the score changed and if so update the LCD score
		//Only update if the score has changed not every frame
		//Save the new left and right scores
		if (scoreLeft != lastScoreLeft || scoreRight != lastScoreRight) {
			lcd_show_score(scoreLeft, scoreRight);
			lastScoreLeft = scoreLeft;
			lastScoreRight = scoreRight;
		}

		static int debugCounter = 0; //Initialise variable to track frames for debug output
		debugCounter++;	//Increment the counter each frame

		//Every 10 fames print the sensor distance
		if ((debugCounter % 10) == 0) {
			float cm;

			//Read the distance from the sensor (HCSR04)
			if (HCSR04_ReadCm(&sensor, &cm)) {

				//The 'cm' is full distance, 'whole' is the integer part
				//Cast 'cm' to an integer to get the whole number
				//Subtract integer part from full distance to get decimal
				//Multiply by 100 to represent part after decimal
				int whole = (int) cm;
				int dec = (int) ((cm - (float) whole) * 100.0f);

				//Print the distance in "NN.NN cm" format
				PRINTF("Distance: %d.%02d cm\r\n", whole, dec);
			} else {
				//If the sensor fails to read a distance print "Distance: ---"
				PRINTF("Distance: ---\r\n");
			}
		}

		//Clear the frame buffer before drawing new frame (set all pixels to 0x00 = black)
		fb_clear(0x00);

		//Draw the borders for the game
		draw_top_border();
		draw_bottom_border();
		draw_side_borders();

		//Draw the left and right paddles at the current position, X always same, Y changes
		draw_paddle(leftPaddleX, leftPaddleTopY, PADDLE_H);
		draw_paddle(rightPaddleX, rightPaddleTopY, PADDLE_H);

		//Draw the ball at its current X,Y position
		draw_ball(ballX, ballY);

		//Push the frame buffer to the OLED display to actually show the updated frame
		fb_flush_to_oled();

		// Add a small delay to make the game playable at a reasonable speed (around 50 Hz refresh rate)
		// This delay is 20 ms (20,000 µs)
		SDK_DelayAtLeastUs(20000u, CLOCK_GetFreq(kCLOCK_CoreSysClk));
	}
}
