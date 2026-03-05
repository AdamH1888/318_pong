#include "fsl_device_registers.h"
#include "fsl_clock.h"
#include "fsl_lpi2c.h"              //LPI2C driver (used for OLED and LCD)
#include "fsl_port.h"
#include "fsl_gpio.h"
#include "clock_config.h"
#include "board.h"
#include "app.h"
#include "fsl_debug_console.h"
#include "fsl_common.h"            //SDK_DelayAtLeastUs
#include "i2c_bus.h"                //Small helper module used by OLED and LCD (Share same bus)
#include "oled.h"                   //OLED driver
#include "lcd_score.h"              //Score display on the 16x2 character LCD
#include "distance_sensor.h"        //HC-SR04 distance sensor driver
#include "pot.h"                    //Potentiometer (ADC) driver
#include "game_config.h"            //Game settings (screen size, paddle size, speeds, enums etc)
#include "game_logic.h"             //Physics/collisions/scoring/AI logic
#include "framebuffer.h"            //Buffer in RAM that represents OLED pixels
#include "draw.h"                   //Drawing paddles/ball/border
#include <stdint.h>
#include <stdbool.h>
#include "servo.h"                  //SG90 servo driver
#include "buzzer.h"                 //Buzzer driver (PIO0_31)

#define BUTTON_GPIO              GPIO4  //GPIO port for start/pause button (PIO4_2)
#define BUTTON_PIN               2u     //GPIO pin for button (GPIO4_2, active low with external pull-up)
#define BUTTON_DEBOUNCE_FRAMES   1      //Hardware RC filter handles bounce; 1 frame (20ms) is enough to register a press

#define RESET_GPIO               GPIO4  //GPIO port for reset button (PIO4_3)
#define RESET_PIN                3u     //GPIO pin for reset button (GPIO4_3, active low with external pull-up)

#define SWITCH_GPIO              GPIO0  //GPIO port for mode select switch (PIO0_26)
#define SWITCH_PIN               26u    //GPIO pin for mode select switch (1=Distance Sensor, 0=Potentiometer)

//Enable the CPU cycle counter so frame timing uses real elapsed time instead of assuming every loop is identical
static void initCycleCounter(void) {
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
#ifdef DWT_LAR
	DWT->LAR = 0xC5ACCE55u; //Unlock CYCCNT on devices that protect DWT writes
#endif
	DWT->CYCCNT = 0u;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

//Convert CPU cycles into microseconds (64-bit math avoids overflow during the multiply)
static uint32_t cyclesToUs(uint32_t cycles, uint32_t cpuFreqHz) {
	return (uint32_t) (((uint64_t) cycles * 1000000ULL) / (uint64_t) cpuFreqHz);
}

int main(void) {
	BOARD_InitHardware();

	i2c_bus_init();		//Initialises I2C bus shared by OLED and LCD
	initOLED();			//Initialises OLED display
	lcd_score_init();	//Initialises score system on LCD character display

	//Sensor structs showing what GPIO and pin are used for TRIG and ECHO
	hcsr04_t sensor = { .trigGpio = GPIO0, .trigPin = 29u, .echoGpio = GPIO1,
			.echoPin = 23u, };

	CLOCK_EnableClock(kCLOCK_Gpio0);
	CLOCK_EnableClock(kCLOCK_Gpio1);

	//Initialises this so readings from distance sensors can be taken
	HCSR04_Init(&sensor);

	//Initialise both potentiometers (left = ADC1_A0, right = ADC1_A3)
	Pot_Init();

	//Initialise buzzer on PIO0_31
	Buzzer_Init();

	//Initialise servo on GPIO0_28 (GPIO0 clock already enabled above)
	Servo_Init();
	//Set servo to far left (0 degrees) on startup
	Servo_Update(SERVO_TRACKING, 0);

	//Initialise reset button pin as input (PORT4/GPIO4 clocks already enabled by BOARD_InitPins)
	PORT_SetPinMux(PORT4, RESET_PIN, kPORT_MuxAlt0); //Set PIO4_3 to GPIO function
	PORT4->PCR[RESET_PIN] |= PORT_PCR_IBE(PCR_IBE_ibe1);   //Enable input buffer
	gpio_pin_config_t reset_config = { kGPIO_DigitalInput, 0 };
	GPIO_PinInit(RESET_GPIO, RESET_PIN, &reset_config);

	//Initialise mode select switch pin as input (PIO0_26)
	PORT_SetPinMux(PORT0, SWITCH_PIN, kPORT_MuxAlt0); //Set PIO0_26 to GPIO function
	PORT0->PCR[SWITCH_PIN] |= PORT_PCR_IBE(PCR_IBE_ibe1);   //Enable input buffer
	PORT0->PCR[SWITCH_PIN] |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK; //Enable weak pull-up so the input doesn't float
	gpio_pin_config_t switch_config = { kGPIO_DigitalInput, 0 };
	GPIO_PinInit(SWITCH_GPIO, SWITCH_PIN, &switch_config);

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

	GameState gameState = STATE_MENU_MAIN; //Start on main menu
	GameMode gameMode = MODE_DISTANCE_SENSOR; //Default to distance sensor, will be set by user on mode select

	uint16_t scoreLeft = 0;
	uint16_t scoreRight = 0;

	//LCD is only updated when score changes not every frame, this ensures it updates LCD score immediately to 0-0 at the start
	//0xFFFF is very safe as every bit turned on (65535), so last score never equals score at the start
	uint16_t lastScoreLeft = 0xFFFFu;
	uint16_t lastScoreRight = 0xFFFFu;

	//Timer: counts down from TIMER_TOTAL_SECONDS to 0, only decrements while in STATE_RUNNING
	//Pretend the previous time something completely different, so the game is forced to draw the timer the first time it runs
	uint32_t timerSeconds = TIMER_TOTAL_SECONDS;
	uint32_t lastTimerSecs = 0xFFFFFFFFu;    //Force first draw when game starts
	uint32_t timerAccumUs = 0u; //Accumulate real elapsed time while the game is running
	uint32_t gameOverFrames = 0u; //Frame counter for Game Over screen display duration

	uint32_t cpu_freq = CLOCK_GetFreq(kCLOCK_CoreSysClk);
	initCycleCounter();
	uint32_t lastLoopCycle = DWT->CYCCNT;
	bool wasRunningLastLoop = false;

	while (1) {
		uint32_t loopStartCycle = DWT->CYCCNT;
		uint32_t elapsedUs = cyclesToUs(loopStartCycle - lastLoopCycle, cpu_freq);
		lastLoopCycle = loopStartCycle;

		//Countdown uses real elapsed time, so LCD stays accurate even if I2C/sensor/debug work makes some frames longer
		if (wasRunningLastLoop && timerSeconds > 0u) {
			timerAccumUs += elapsedUs;
			while (timerAccumUs >= 1500000u && timerSeconds > 0u) {
				timerAccumUs -= 1500000u;
				timerSeconds--;
				if (timerSeconds == 0u) {
					gameState = STATE_GAME_OVER;
					gameOverFrames = 0u;
					break;
				}
			}
		}

		frameCount++;

		//Read start/pause button (active low: pin reads 0 when pressed against pull-up)
		//Debounce: button state must be stable for BUTTON_DEBOUNCE_FRAMES consecutive frames
		static bool btnPrev = true; //Last stable button state (true = not pressed)
		static uint32_t btnDebounce = 0; //Count of frames where raw state differs from stable state
		static bool menuNeedsRender = true; //Flag to trigger menu screen redraw
		static bool modeNeedsRender = true; //Flag to trigger mode select screen redraw
		bool btnRaw = (bool) GPIO_PinRead(BUTTON_GPIO, BUTTON_PIN); //1=released, 0=pressed
		if (btnRaw != btnPrev) {
			btnDebounce++;
			if (btnDebounce >= BUTTON_DEBOUNCE_FRAMES) { //Stable for enough frames
				btnPrev = btnRaw;
				btnDebounce = 0;
				if (!btnRaw) {     //Falling edge detected = button just pressed
					if (gameState == STATE_MENU_MAIN) {
						gameState = STATE_MODE_SELECT; //Enter gamemode selection
						modeNeedsRender = true; //Trigger mode select screen render
					} else if (gameState == STATE_MODE_SELECT) {
						//Confirm mode selection and start game
						gameState = STATE_RUNNING;
						servePauseFrames = SERVE_INITIAL_FRAMES;
						timerSeconds = TIMER_TOTAL_SECONDS;
						lastTimerSecs = 0xFFFFFFFFu;
						timerAccumUs = 0u;
						gameOverFrames = 0u;
						lastScoreLeft = scoreLeft; //Sync score so LCD updates but buzzer doesn't fire
						lastScoreRight = scoreRight;
					} else if (gameState == STATE_RUNNING) {
						gameState = STATE_PAUSED;   //Press during game: pause
					} else if (gameState == STATE_PAUSED) {
						gameState = STATE_RUNNING;  //Press while paused: resume
					}
				}
			}
		} else {
			btnDebounce = 0;  //Reset debounce counter whenever state is stable
		}

		//Reset button: RESET from any game state returns to main menu
		static bool resetPrev = true;
		static uint32_t resetDebounce = 0;
		bool resetRaw = (bool) GPIO_PinRead(RESET_GPIO, RESET_PIN);
		if (resetRaw != resetPrev) {
			resetDebounce++;
			if (resetDebounce >= BUTTON_DEBOUNCE_FRAMES) {
				resetPrev = resetRaw;
				resetDebounce = 0;
				if (!resetRaw) {  //Button just pressed
					if (gameState == STATE_RUNNING
							|| gameState == STATE_PAUSED
							|| gameState == STATE_GAME_OVER
							|| gameState == STATE_MODE_SELECT) {
						//Return to main menu from any game state
						gameState = STATE_MENU_MAIN;
						menuNeedsRender = true;       //Trigger main menu redraw
						scoreLeft = 0;
						scoreRight = 0;
						lastScoreLeft = 0xFFFFu;
						lastScoreRight = 0xFFFFu;
						ballX = BALL_SPAWN_LEFT_X;
						ballY = BALL_SPAWN_Y;
						ballVelocityX = 4;
						ballVelocityY = 2;
						servePauseFrames = 0;
						leftPaddleTopY = 24;
						rightPaddleTopY = 24;
						timerSeconds = TIMER_TOTAL_SECONDS;
						lastTimerSecs = 0xFFFFFFFFu;
						timerAccumUs = 0u;
						gameOverFrames = 0u;
						lcd_show_score(0, 0);
						lcd_clear_timer();
						Buzzer_Stop();
					}
				}
			}
		} else {
			resetDebounce = 0;
		}

		//Read mode select switch every frame so the control method can be changed immediately
		GameMode switchMode =
				GPIO_PinRead(SWITCH_GPIO, SWITCH_PIN) ?
						MODE_DISTANCE_SENSOR : MODE_POTENTIOMETER;
		if (switchMode != gameMode) {
			gameMode = switchMode;
			if (gameState == STATE_MODE_SELECT) {
				modeNeedsRender = true; //Refresh highlight when the switch changes on the selection screen
			}
		}

		int ballCenterY = ballY + 1; //The ball is 2 pixels tall so adding 1 gives bottom pixel ball value

		//Refresh buzzer output every frame so the beep length is non-blocking
		Buzzer_Update();

		//Left paddle controlled by distance sensor OR potentiometer based on gameMode
		//Always update so player can position paddle before/after game
		static int sensorFrameCounter = 0;
		sensorFrameCounter++;
		static float lastSensorCm = 0.0f; //Most recent valid raw reading (for debug print)
		static bool lastSensorValid = false; //True once at least one valid reading has been received

		if (gameMode == MODE_DISTANCE_SENSOR) {
			//Distance sensor mode with EMA smoothing
			if (sensorFrameCounter >= 3) { //Read sensor every 3 frames (60ms)
				sensorFrameCounter = 0;

				float cm;
				static float smoothedCm = 12.0f; //EMA smoothed distance (starting value middle of 6 and 18cm)

				if (HCSR04_ReadCm(&sensor, &cm)) {
					if (cm < 6.0f)
						cm = 6.0f;
					if (cm > 18.0f)
						cm = 18.0f;
					smoothedCm = 0.35f * cm + 0.65f * smoothedCm; //EMA smoothing
					leftPaddleTopY = HCSR04_MapToPaddleY(smoothedCm);
					lastSensorCm = cm;
					lastSensorValid = true;
				}
			}
		} else {
			//Left potentiometer mode with hysteresis to reduce paddle jitter
			if (sensorFrameCounter >= 1) { //Fast update every frame
				sensorFrameCounter = 0;

				int32_t potRaw = Pot_ReadRaw();
				if (potRaw >= 0) {
					static int32_t lastPotRaw = -1;
					if (lastPotRaw < 0 || (potRaw - lastPotRaw > 50)
							|| (lastPotRaw - potRaw > 50)) {
						lastPotRaw = potRaw;
						leftPaddleTopY = Pot_MapToPaddleY(
								(uint32_t) potRaw);
					}
					lastSensorCm = (float) potRaw / 4095.0f * 12.0f; //Fake cm value for display
					lastSensorValid = true;
				}
			}
		}

		//Right paddle behaviour depends on the selected switch position
		//Potentiometer mode: right paddle uses potentiometer 2 (ADC1_A3)
		//Distance sensor mode: right paddle is handled by AI while the game is running
		if (gameMode == MODE_POTENTIOMETER) {
			static int rightPotFrameCounter = 0;
			rightPotFrameCounter++;

			if (rightPotFrameCounter >= 2) { //Read every 2 frames to avoid ADC conflict with left pot
				rightPotFrameCounter = 0;

				int32_t rightPotRaw = Pot_ReadRightRaw();
				if (rightPotRaw >= 0) {
					static int32_t lastRightPotRaw = -1;

					if (lastRightPotRaw < 0 || (rightPotRaw - lastRightPotRaw > 50)
							|| (lastRightPotRaw - rightPotRaw > 50)) {
						lastRightPotRaw = rightPotRaw;
						rightPaddleTopY = Pot_MapToPaddleY(
								(uint32_t) rightPotRaw);
					}
				}
			}
		}

		//All game updates (AI, ball movement, collisions, scoring) only run while game is live
		if (gameState == STATE_RUNNING) {

			//In distance sensor mode, the right paddle is controlled by AI.
			if (gameMode == MODE_DISTANCE_SENSOR) {
				updateAiPaddle(&rightPaddleTopY, ballCenterY, frameCount);
			}

			// After someone scores, you pause the serve for a few frames, value set in game logic file
			// If we're still in the pause, count down
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
				//Ball hits the paddle
				//Adjust angle based on which third of paddle was hit
				adjustBallAngleFromPaddleHit(&ballVelocityY, ballCenterY, leftPaddleTopY);

				//Move ball slightly away from paddle so it doesn't stick
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
				//Adjust angle based on which third of paddle was hit
				adjustBallAngleFromPaddleHit(&ballVelocityY, ballCenterY, rightPaddleTopY);

				//Move the ball away from paddle so it doesn't get stuck
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
				Buzzer_Beep(BUZZER_BEEP_FRAMES); //Trigger beep on point scored
			}


		} //end if (gameState == STATE_RUNNING)

		//Game Over screen timeout: return to menu after a short delay
		//GAME_OVER_DISPLAY_MS (3000ms) ÷ 20ms/frame = 150 frames
		if (gameState == STATE_GAME_OVER) {
			gameOverFrames++;
			if (gameOverFrames >= (GAME_OVER_DISPLAY_MS / 20u)) { //3000ms ÷ 20ms = 150 frames
				gameState = STATE_MENU_MAIN;
				lcd_clear_timer();
				lastTimerSecs = 0xFFFFFFFFu;
			}
		}

		//Update timer on LCD row 2 whenever seconds value changes (runs outside STATE_RUNNING
		//so the last value stays visible while paused, not shown at all on menu)
		if (timerSeconds != lastTimerSecs
				&& (gameState == STATE_RUNNING || gameState == STATE_PAUSED)) {
			lcd_show_timer(timerSeconds);
			lastTimerSecs = timerSeconds;
		}

		static int debugCounter = 0; //Initialise variable to track frames for debug output
		debugCounter++;	//Increment the counter each frame

		//Every 10 frames print debug info based on active mode
		if ((debugCounter % 10) == 0) {
			if (gameMode == MODE_DISTANCE_SENSOR) {
				if (lastSensorValid) {
					int whole = (int) lastSensorCm;
					int dec = (int) ((lastSensorCm - (float) whole) * 100.0f);
					PRINTF("Distance Sensor: %d.%02d cm\r\n", whole, dec);
				} else {
					PRINTF("Distance Sensor: ---\r\n");
				}
			} else {
				// Left potentiometer mode - read current value for debug
				int32_t potRaw = Pot_ReadRaw();
				if (potRaw >= 0) {
					PRINTF("Left Potentiometer: %ld / 4095\r\n", (long) potRaw);
				} else {
					PRINTF("Left Potentiometer: Read Error!\r\n");
				}
			}

			if (gameMode == MODE_POTENTIOMETER) {
				//In potentiometer mode the right paddle uses potentiometer 2
				int32_t rightPotRaw = Pot_ReadRightRaw();
				if (rightPotRaw >= 0) {
					PRINTF("Right Pot: %ld / 4095 -> Y=%d\r\n", (long) rightPotRaw, rightPaddleTopY);
				} else {
					PRINTF("Right Pot: TIMEOUT/ERROR\r\n");
				}
			} else {
				PRINTF("Right Paddle: AI (Y=%d)\r\n", rightPaddleTopY);
			}
		}

		//Render: menu uses direct OLED text writes; game uses the framebuffer
		//Render flags ensure menu text is only written once (avoids flicker from redrawing every frame)
		static bool gameOverNeedsRender = true;

		if (gameState == STATE_MENU_MAIN) {
			if (menuNeedsRender) {
				fillOLED(0x00);                              //Clear the display
				writeString((char*) "PONG", false, 52, 2); //Title, centred
				writeString((char*) "Press START", false, 22, 4); //Go to mode select
				writeString((char*) "to select mode", false, 22, 5);
				menuNeedsRender = false;
			}
			gameOverNeedsRender = true;
		} else if (gameState == STATE_MODE_SELECT) {
			//Mode selection screen with cursor highlighting
			if (modeNeedsRender) {
				fillOLED(0x00);                                  //Clear display
				writeString((char*) "SELECT MODE", false, 31, 1); //Title, centred (11 chars × 6px = 66px, (128-66)/2 = 31)

				//Show Distance Sensor option with highlight if selected
				if (gameMode == MODE_DISTANCE_SENSOR) {
					writeString((char*) ">>Distance Sensor<<", false, 7, 3);  //Highlighted, centred (19 chars)
				} else {
					writeString((char*) "Distance Sensor", false, 19, 3);     //Normal, centred (15 chars)
				}

				//Show Potentiometer option with highlight if selected
				if (gameMode == MODE_POTENTIOMETER) {
					writeString((char*) ">>Potentiometer<<", false, 13, 5);   //Highlighted, centred (17 chars)
				} else {
					writeString((char*) "Potentiometer", false, 25, 5);       //Normal, centred (13 chars)
				}

				modeNeedsRender = false;
			}
			gameOverNeedsRender = true;
		} else if (gameState == STATE_GAME_OVER) {
			menuNeedsRender = true;     //Trigger main menu redraw on return
			modeNeedsRender = true; //Ensure mode select can render if entered again
			if (gameOverNeedsRender) {
				fillOLED(0x00); //Clear the display
				writeString((char*) "GAME OVER", false, 34, 2);
				writeString((char*) "Returning to menu", false, 7, 4);
				gameOverNeedsRender = false;
			}
		} else {
			menuNeedsRender = true; //Reset so menu redraws correctly if ever re-entered
			modeNeedsRender = true; //Reset so mode select redraws if entered again
			gameOverNeedsRender = true;

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
		}

		//SERVO_OFF:      no pulse (menu, motor de-energizes)
		ServoState servoState;
		if (gameState == STATE_RUNNING) {
			servoState = SERVO_TRACKING;
		} else if (gameState == STATE_PAUSED) {
			servoState = SERVO_HOLD;
		} else {
			servoState = SERVO_OFF;
		}
		Servo_Update(servoState, timerSeconds);

		//Keep the full frame close to 20ms by subtracting the ACTUAL work time of this loop
		uint32_t workUs = cyclesToUs(DWT->CYCCNT - loopStartCycle, cpu_freq);
		uint32_t remaining_us = (workUs < FRAME_PERIOD_US) ? (FRAME_PERIOD_US - workUs) : 0u;
		if (remaining_us > 0u) {
			SDK_DelayAtLeastUs(remaining_us, cpu_freq);
		}

		wasRunningLastLoop = (gameState == STATE_RUNNING);
	}
}
