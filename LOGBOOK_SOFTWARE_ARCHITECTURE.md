# PONG Game - Software Architecture & Technical Logbook

## Project Overview
This is an embedded PONG game running on the FRDM-MCXN947 microcontroller. The game features two paddles (player-controlled and AI/potentiometer), a ball with physics simulation, real-time scoring, countdown timer, and multiple input/output peripherals.

---

## Table of Contents
1. [Architecture Overview](#architecture-overview)
2. [Game State Machine](#game-state-machine)
3. [Game Loop & Real-Time Timing](#game-loop--real-time-timing)
4. [Physics & Collision System](#physics--collision-system)
5. [Input Systems](#input-systems)
6. [AI Implementation](#ai-implementation)
7. [Software Debouncing](#software-debouncing)
8. [Output Systems](#output-systems)
9. [Rendering Pipeline](#rendering-pipeline)
10. [Advanced Techniques](#advanced-techniques)

---

## Architecture Overview

### High-Level System Design
```
┌─────────────────────────────────────────────────────────┐
│                     MAIN GAME LOOP                      │
│  Runs at 50 Hz (20ms frame period) with real timing    │
└────────────────────┬────────────────────────────────────┘
                     │
        ┌────────────┴────────────┐
        │                         │
   ┌────▼──────────┐     ┌───────▼──────┐
   │ Input Reading │     │ Game Physics │
   ├───────────────┤     ├──────────────┤
   │- Buttons      │     │- Ball movement
   │- Sensors      │     │- Paddle update
   │- Potentiometers
   │- Mode switch  │     │- Collisions
   └───────────────┘     │- Scoring
                         └──────────────┘
                                │
                         ┌──────▼──────────┐
                         │ Output Drivers  │
                         ├─────────────────┤
                         │- Buzzer         │
                         │- LCD Display    │
                         │- OLED Display   │
                         │- Servo Motor    │
                         └─────────────────┘
```

### Component Responsibilities
- **main.c**: Central game loop, state transitions, input/output coordination
- **game_logic.c**: Collision detection, ball physics, scoring logic
- **servo.c**: PWM timing for servo motor control
- **distance_sensor.c**: HC-SR04 sensor measurement with microsecond-precision timing
- **pot.c**: ADC sampling for potentiometer inputs
- **buzzer.c**: Non-blocking audio feedback
- **framebuffer.c**: RAM-based pixel buffer for OLED
- **draw.c**: Graphics rendering functions
- **i2c_bus.c**: Shared I2C communication for OLED and LCD displays
- **oled.c / lcd_score.c**: Display interface libraries

---

## Game State Machine

### State Definitions
The game operates in 5 distinct states using an enumerated type pattern:

```c
typedef enum {
    STATE_MENU_MAIN,    // Main menu with "PONG" title
    STATE_MODE_SELECT,  // Choose between Distance Sensor or Potentiometer mode
    STATE_RUNNING,      // Active gameplay
    STATE_PAUSED,       // Game frozen, awaiting resume
    STATE_GAME_OVER     // Timer expired, showing end screen
} GameState;
```

### State Transitions

```
┌─────────────┐
│ MENU_MAIN   │◄──────────────────────────────────┐
│ (initial)   │                                   │
└──────┬──────┘                            RESET Button
       │ START Button
       │
       ▼
┌──────────────┐
│ MODE_SELECT  │
│ (Distance or │
│  Pot choice) │
└──────┬───────┘
       │ START Button (confirm)
       │
       ▼
┌──────────────┐      Pause Button         ┌───────────┐
│   RUNNING    │───────────────────────►  │  PAUSED   │
│   (gameplay) │                           │(frozen)   │
└──────┬───────┘◄───────────────────────  └─────┬─────┘
       │                Pause Button           Resume
       │ Timer reaches 0 sec
       │
       ▼
┌──────────────┐  3 seconds later   ┌─────────────┐
│  GAME_OVER   │────────────────────►│ MENU_MAIN   │
│ (End screen) │   Auto-transition    │(restart)    │
└──────────────┘                      └─────────────┘

(RESET button can jump to MENU_MAIN from any game state at any time)
```

### Why This Design?
- **Modularity**: Each state has distinct responsibilities, making code predictable
- **Non-blocking**: No state transition blocks for I/O; transitions happen between frames
- **Clear semantics**: Game behavior is explicit based on current state
- **Debug-friendly**: Easy to see exactly what should happen in each state

---

## Game Loop & Real-Time Timing

### The 20ms Frame Architecture
The game runs at 50 FPS with exactly 20ms per frame. This is critical for:
- Servo motor PWM timing (20ms periods)
- Consistent game physics (fixed timestep)
- Predictable input sampling
- Proper debouncing timing

```c
#define FRAME_PERIOD_US 20000u  // 20ms = 50 Hz
```

### Real Elapsed Time Tracking (DWT Cycle Counter)

**Problem**: Embedded systems don't have perfect frame timing. I2C reads, sensor measurements, and display updates add variable delays. If the game assumed every frame is exactly 20ms, the timer would be inaccurate.

**Solution**: Use the ARM Cortex-M33's Data Watchpoint and Trace (DWT) cycle counter—a dedicated hardware timer that tracks CPU clock cycles with nanosecond precision.

```c
// Enable the cycle counter
static void initCycleCounter(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

// In the game loop
uint32_t loopStartCycle = DWT->CYCCNT;
uint32_t elapsedUs = cyclesToUs(loopStartCycle - lastLoopCycle, cpu_freq);

// Accumulate real elapsed time for the countdown timer
if (wasRunningLastLoop && timerSeconds > 0u) {
    timerAccumUs += elapsedUs;
    while (timerAccumUs >= 1500000u && timerSeconds > 0u) {
        timerAccumUs -= 1500000u;  // 1.5 seconds = 1 second elapsed
        timerSeconds--;
    }
}
```

**Why 1.5 seconds in code = 1 real second?** The game loop occasionally spends extra time on I2C operations (OLED/LCD updates) and sensor reads. Empirically, the loop was taking 1.5 seconds of CPU time for every 1 second of real time. This calibration factor makes the timer accurate.

### Frame Timing Regulation

At the end of each frame:
1. Calculate how long the frame actually took
2. Sleep for the remaining time to hit the 20ms target
3. No busy-waiting; the CPU goes into low-power mode

```c
uint32_t workUs = cyclesToUs(DWT->CYCCNT - loopStartCycle, cpu_freq);
uint32_t remaining_us = (workUs < FRAME_PERIOD_US) ? (FRAME_PERIOD_US - workUs) : 0u;
if (remaining_us > 0u) {
    SDK_DelayAtLeastUs(remaining_us, cpu_freq);  // Sleep for remaining time
}
```

**Benefit**: Even though I2C or sensor reads vary, the overall frame rate stays at exactly 50 FPS.

---

## Physics & Collision System

### Ball Movement
The ball has two velocity components (in pixels per frame):
```c
int ballVelocityX;  // Horizontal: ±4 pixels/frame (moves left/right)
int ballVelocityY;  // Vertical: -2, 0, or +2 pixels/frame (moves up, flat, down)
```

Each frame:
```c
ballX += ballVelocityX;
ballY += ballVelocityY;
```

### Wall Collision (Elastic Bounce)
Top and bottom walls simply reverse the Y velocity:

```c
if (ballY <= 1) {
    ballY = 1;                      // Clamp to boundary
    ballVelocityY = -ballVelocityY; // Reverse direction
}

if (ballY + 1 >= 62) {  // Ball is 2 pixels tall
    ballY = 61;
    ballVelocityY = -ballVelocityY;
}
```

**Why clamp position?** Without clamping, the ball could skip past the wall in a single frame, causing it to get "stuck" bouncing.

### Paddle Collision - Three-Zone Angle System

When the ball hits a paddle, the collision location determines the rebound angle using a 3-zone system:

```c
static void adjustBallAngleFromPaddleHit(int *ballVelocityY, int ballCenterY, int paddleTopY) {
    const int paddleThird = PADDLE_H / 3;  // Each zone is 6 pixels (18/3)
    int hitOffset = ballCenterY - paddleTopY;  // 0=top, 17=bottom

    if (hitOffset < paddleThird) {
        *ballVelocityY = -2;  // TOP THIRD: Strong upward angle
    } else if (hitOffset >= (paddleThird * 2)) {
        *ballVelocityY = 2;   // BOTTOM THIRD: Strong downward angle
    } else {
        *ballVelocityY = 0;   // MIDDLE THIRD: Flat bounce
    }
}
```

**Game Design**: This allows skilled players to aim the ball by hitting the paddle at the edges, adding strategy to gameplay.

```
Paddle hit location effect:
       ┌─────────┐
TOP    │ ↖ ↑ ↗   │  Upward angles (-2, -2, -2)
zone   │─────────┤
       │   →     │  Flat (0)
MID    │─────────┤
       │   ↘     │  Downward angles (2, 2, 2)
zone   │ ↙ ↓ ↘   │
BOT    └─────────┘
```

### Overlap Detection - Core Collision Algorithm

Before bouncing, the code checks if the ball and paddle actually overlap:

```c
bool verticalRangesOverlap(int topA, int bottomA, int topB, int bottomB) {
    return !(bottomA < topB || bottomB < topA);
}

// Usage: ball spans ballY to ballY+1, paddle spans leftPaddleTopY to leftPaddleTopY+PADDLE_H-1
bool hitLeft = verticalRangesOverlap(ballY, ballY + 1,
                                     leftPaddleTopY, leftPaddleTopY + (PADDLE_H - 1));
```

**Logic**: Two ranges DON'T overlap only if one ends before the other starts. Negate that condition to detect overlap.

### Scoring & Ball Reset

When a paddle misses, the opponent scores and the ball resets:

```c
void resetBallAfterPoint(int *ballX, int *ballY, int *vx, int *vy,
                         bool serveToRight, int *servePauseFrames) {
    if (serveToRight) {
        *ballX = BALL_SPAWN_LEFT_X;   // Left side
        *vx = 4;                       // Move right
    } else {
        *ballX = BALL_SPAWN_RIGHT_X;  // Right side
        *vx = -4;                      // Move left
    }
    
    *ballY = BALL_SPAWN_Y;             // Center vertically
    *vy = 2;                           // Downward
    *servePauseFrames = SERVE_PAUSE_FRAMES;  // Pause before ball moves
}
```

**Why the serve pause?** Gives players a moment to position the paddle before the ball comes to them.

---

## Input Systems

### Buttons: START and RESET

Two buttons handle game control:
- **START** (GPIO4_2): Advances menu → mode select → game start, pause/resume during gameplay
- **RESET** (GPIO4_3): Jump back to main menu from any state

Raw GPIO reads are unreliable due to switch bounce (electrical noise). The solution: **software debouncing**.

```c
static bool btnPrev = true;        // Last stable state
static uint32_t btnDebounce = 0;   // Frame counter
static bool menuNeedsRender = true;

bool btnRaw = (bool) GPIO_PinRead(BUTTON_GPIO, BUTTON_PIN);  // 1=released, 0=pressed

if (btnRaw != btnPrev) {
    btnDebounce++;
    if (btnDebounce >= BUTTON_DEBOUNCE_FRAMES) {  // Must be stable for 1+ frames
        btnPrev = btnRaw;                          // Update stable state
        btnDebounce = 0;
        if (!btnRaw) {  // Falling edge (button just pressed)
            // Handle press...
        }
    }
} else {
    btnDebounce = 0;  // Reset counter if state is stable
}
```

(See [Software Debouncing](#software-debouncing) section for detailed explanation.)

### Mode Select Switch

Selects between two gameplay modes:
- **Distance Sensor Mode** (switch = 1): Left paddle controlled by HC-SR04 ultrasonic sensor, right paddle is AI
- **Potentiometer Mode** (switch = 0): Both paddles controlled by potentiometers (ADC)

```c
GameMode switchMode = GPIO_PinRead(SWITCH_GPIO, SWITCH_PIN) ?
                      MODE_DISTANCE_SENSOR : MODE_POTENTIOMETER;
```

The switch is read every frame (unlike buttons which need debouncing because it's a toggle, not momentary).

### Distance Sensor: HC-SR04 Ultrasonic

**Principle**: Send a sound pulse, measure echo time, calculate distance.

```
Measurement pulse timing:
1. Send 10µs HIGH pulse on TRIG pin (triggers sensor)
2. Wait for ECHO pin to go HIGH (start of return echo)
3. Measure time until ECHO goes LOW (end of echo)
4. Distance = (echo_time × speed_of_sound) / 2
   (÷2 because sound travels to object and back)
```

**Hardware constraints**: Ultrasonic sensors are slow (~60ms per measurement). The game can't block on every read.

**Solution**: Read the sensor every 3 frames (60ms), smoothing readings with exponential moving average (EMA):

```c
if (sensorFrameCounter >= 3) {  // Read every 3 frames
    sensorFrameCounter = 0;
    
    float cm;
    static float smoothedCm = 12.0f;  // Initial: middle of 6-18cm range
    
    if (HCSR04_ReadCm(&sensor, &cm)) {
        // Clamp to valid range
        if (cm < 6.0f) cm = 6.0f;
        if (cm > 18.0f) cm = 18.0f;
        
        // EMA filter: 35% new reading, 65% previous value
        smoothedCm = 0.35f * cm + 0.65f * smoothedCm;
        
        leftPaddleTopY = mapDistanceCmToPaddleY(smoothedCm);
    }
}
```

**EMA Smoothing Benefit**: Reduces sensor noise. A sudden spike from 10cm → 15cm gets blended:
- Raw: instant jump
- EMA: gradual 10 → 11.75 → 12.81 → ...

**Distance-to-Position Mapping**:
```c
static int mapDistanceCmToPaddleY(float cm) {
    const float cmNear = 6.0f;   // Closest (hand near sensor) → bottom of screen
    const float cmFar = 18.0f;   // Farthest (hand far from sensor) → top of screen
    
    // Clamp to valid range
    if (cm < cmNear) cm = cmNear;
    if (cm > cmFar) cm = cmFar;
    
    // Normalize to 0.0-1.0
    float t = (cm - cmNear) / (cmFar - cmNear);
    
    // Map to paddle Y range (1 = top, 44 = bottom, accounting for palette height)
    const int yMin = 1;
    const int yMax = 62 - PADDLE_H;
    
    int y = (int) ((1.0f - t) * (yMax - yMin) + yMin);
    return y;
}
```

**Why invert t with (1.0f - t)?** Intuitive control: hand closer = paddle moves down (intuitively toward the sensor).

### Potentiometer: ADC Analog Input

Two potentiometers on ADC1:
- **Left paddle**: ADC1_A0 (channel 0)
- **Right paddle**: ADC1_A3 (channel 3)

Raw 12-bit ADC values (0-4095) are read and mapped to paddle Y position:

```c
static int mapPotentiometerTopaddleY(uint32_t potValue) {
    // Normalize ADC reading to 0.0-1.0
    float t = (float) potValue / 4095.0f;
    
    const int yMin = 1;
    const int yMax = 62 - PADDLE_H;
    
    // Higher ADC value = paddle at top of screen
    int y = (int) ((1.0f - t) * (yMax - yMin) + yMin);
    return y;
}
```

**Hysteresis (Jitter Reduction)**: Potentiometers are noisy. Reading 2048 one frame and 2050 the next causes paddle flicker. Solution: only update if change > 50 ADC units:

```c
if (lastPotRaw < 0 || (potRaw - lastPotRaw > 50) || (lastPotRaw - potRaw > 50)) {
    lastPotRaw = potRaw;
    leftPaddleTopY = mapPotentiometerTopaddleY((uint32_t) potRaw);
}
```

**Update Rate**: Left pot read every frame, right pot read every 2 frames to avoid ADC conflicts.

---

## AI Implementation

When the game is in **Distance Sensor Mode**, the right paddle is controlled by artificial intelligence:

```c
static void updateAiPaddle(int *paddleTopY, int ballCenterY, int frameCount) {
    // AI reacts every N frames (doesn't update every single frame for performance)
    if ((frameCount % AI_REACT_EVERY_N_FRAMES) != 0)
        return;
    
    // Occasionally the AI "hesitates" and does nothing (adds unpredictability)
    if ((rngNext() % 100u) < AI_HESITATE_PERCENT)
        return;
    
    // Calculate paddle center and difference from ball center
    int paddleCenterY = *paddleTopY + (PADDLE_H / 2);
    int diff = ballCenterY - paddleCenterY;
    
    // Only move if ball is far enough away from paddle center (deadzone)
    if (diff > AI_DEADZONE_PIXELS)
        (*paddleTopY)++;  // Move down one pixel
    if (diff < -AI_DEADZONE_PIXELS)
        (*paddleTopY)--;  // Move up one pixel
    
    // Keep paddle in bounds
    *paddleTopY = clampValueToRange(*paddleTopY, 1, 62 - PADDLE_H);
}
```

### AI Complexity Tuning Parameters

```c
#define AI_REACT_EVERY_N_FRAMES  5       // Reacts every 5 frames (slow reaction)
#define AI_DEADZONE_PIXELS       2       // Ignores if ball within 2 pixels of paddle center
#define AI_HESITATE_PERCENT      25      // 25% chance to do nothing each reaction
```

**Why these design choices?**
1. **Reaction delay** (every N frames): Makes AI beatable by humans. Instant reaction would be unbeatable.
2. **Deadzone**: Helps AI stay centered even when ball is nearby; smoother play.
3. **Hesitation**: Random moments of "AI stupidity" keep the game unpredictable and fun rather than mechanical.

### Linear Congruential Generator (LCG) for Random Numbers

Pseudo-random numbers for hesitation:

```c
static uint32_t rngState = 0x12345678u;
static uint32_t rngNext(void) {
    rngState = (1103515245u * rngState + 12345u);  // Standard LCG formula
    return rngState;
}

// Usage: if (rngNext() % 100u < AI_HESITATE_PERCENT)
```

**Why not `rand()` from stdlib?** Embedded systems often don't have floating-point math library, or it's too heavy. LCG is minimal (one multiplication, one addition) and provides sufficient randomness for game AI.

---

## Software Debouncing

### The Problem: Contact Bounce

When a switch closes, the electrical contact doesn't settle instantly. It "bounces" between connected and disconnected states for 10-50ms:

```
Ideal switch press:        Real switch press:
                           ─────────────
   ──┐                     ──┐  ┌─┬─┐ ┌──
     │                       ├──┘ └─┘ └─ (noise)
     └─────────────           (bounce zone: 20-50ms)
```

Without debouncing, a single button press could be read as multiple presses.

### Software Debouncing Algorithm

Store both the current raw reading and the last stable reading. Only update the stable reading when the raw reading is stable for N consecutive frames:

```c
static bool btnPrev = true;        // Last confirmed state
static uint32_t btnDebounce = 0;   // Frame counter
bool btnRaw = (bool) GPIO_PinRead(BUTTON_GPIO, BUTTON_PIN);

if (btnRaw != btnPrev) {
    btnDebounce++;
    if (btnDebounce >= BUTTON_DEBOUNCE_FRAMES) {  // 1 frame = 20ms = enough to settle
        btnPrev = btnRaw;              // Accept new stable state
        btnDebounce = 0;               // Reset counter
        
        if (!btnRaw) {  // Only act on falling edge (raw went from 1 to 0)
            // Handle button press...
        }
    }
} else {
    btnDebounce = 0;  // Reset counter if reading is already stable
}
```

### Why Only React to Falling Edge?

```
GPIO reads (1=released, 0=pressed):
Time:     0   1   2   3   4   5
Raw:      1   0   0   0   0   1
Stable:   1   1   1   0   0   0

After frame 3:
- btnRaw changed from 1 to 0
- Counter >= BUTTON_DEBOUNCE_FRAMES
- btnPrev becomes 0 (now stable)
- We detect falling edge (1→0) and handle the press
```

Only checking for the **falling edge** (pressed state) ensures:
1. Single action per press (not repeated on release)
2. Clean state transitions
3. No accidental double-presses from bounce

---

## Output Systems

### Buzzer: Non-Blocking Beep-on-Demand

The buzzer plays a tone for a specific number of frames without blocking the game loop.

**Principle**: Count down frames each update; buzzer is ON while counter > 0.

```c
static uint32_t s_buzzerFramesRemaining = 0u;

void Buzzer_Update(void) {
    if (s_buzzerFramesRemaining > 0u) {
        s_buzzerFramesRemaining--;
        Buzzer_Write(true);   // Turn on
    } else {
        Buzzer_Write(false);  // Turn off
    }
}

void Buzzer_Beep(uint32_t frames) {
    s_buzzerFramesRemaining = frames;
}

// Usage in main loop
if (scoreLeft != lastScoreLeft || scoreRight != lastScoreRight) {
    lcd_show_score(scoreLeft, scoreRight);
    Buzzer_Beep(BUZZER_BEEP_FRAMES);  // 3 frames ≈ 60ms beep
}
```

**State-machine approach**: The buzzer isn't aware of the game; it just counts down frames. This is **decoupled** from game logic—the game doesn't need to wait for sound to finish.

### LCD Display: Character-Based Score

A 16×2 character LCD shows the score (e.g., "2 - 1" and timer on second row).

Updated only when score changes (not every frame):

```c
if (scoreLeft != lastScoreLeft || scoreRight != lastScoreRight) {
    lcd_show_score(scoreLeft, scoreRight);
    lastScoreLeft = scoreLeft;
    lastScoreRight = scoreRight;
}
```

**Optimization**: Redraws are expensive (I2C communication). Only redraw when data actually changes.

Timer displayed separately:

```c
if (timerSeconds != lastTimerSecs && 
    (gameState == STATE_RUNNING || gameState == STATE_PAUSED)) {
    lcd_show_timer(timerSeconds);
    lastTimerSecs = timerSeconds;
}
```

### OLED Display: Graphics Menus

A monochrome 128×64 pixel OLED shows:
- **Main menu**: "PONG" title
- **Mode select**: Highlighted choice with ">>" markers
- **Game over**: "GAME OVER" and return message
- **Gameplay**: Game rendered via framebuffer

Direct writes for text (menus); framebuffer for graphics (game).

### Servo Motor: PWM-Controlled Position Tracking

The servo tracks the ball's horizontal position in real-time using PWM (Pulse Width Modulation).

**Standard SG90 Servo Timing**:
```
20ms period (50 Hz)
│◄─────────────────────────────────────────│
 ┌─┐
 │ │ 1000µs  → 0°   (left)
 │ │ 1500µs  → 90°  (center)
 │ │ 2000µs  → 180° (right)
 └─┘
  ↑
 pulse width
```

The game maps ball X position (0-127 pixels) to servo angle (0-180°):

```c
uint32_t Servo_Update(ServoState state, uint32_t timerSeconds) {
    uint32_t cpu_freq = CLOCK_GetFreq(kCLOCK_CoreSysClk);
    
    if (state == SERVO_OFF) {
        GPIO_PinWrite(SERVO_GPIO, SERVO_PIN, 0u);  // No pulse = motor off
        return 0u;
    }
    
    uint16_t angle_x10;  // Angle in units of 0.1 degrees
    
    if (state == SERVO_TRACKING) {
        // Clever game design: servo sweep represents time countdown
        // 30sec = 180°, 0sec = 0°
        uint32_t max_time = 30u;
        if (timerSeconds > max_time) timerSeconds = max_time;
        
        uint16_t base_angle_x10 = (uint16_t)((timerSeconds * 1800u) / max_time);
        
        // Progressive ticking as time runs out (adds tension)
        static uint32_t tick_counter = 0u;
        tick_counter++;
        
        uint32_t tick_interval = 10u;  // Default: slow
        if (timerSeconds < 10u) {
            tick_interval = 1u;  // Last 10 seconds: fast ticking
        } else if (timerSeconds < 20u) {
            tick_interval = 5u;  // 10-20 seconds: medium
        }
        
        // ±15° oscillation
        uint16_t tick_offset = ((tick_counter / tick_interval) % 2u) ? 150u : 0u;
        
        if (timerSeconds < 28u && base_angle_x10 >= tick_offset) {
            angle_x10 = base_angle_x10 - tick_offset;
        } else {
            angle_x10 = base_angle_x10 + tick_offset;
        }
        
        s_last_angle_x10 = angle_x10;
    }
    else {  // SERVO_HOLD
        angle_x10 = s_last_angle_x10;  // Freeze at last position when paused
    }
    
    // Convert angle to pulse width
    uint32_t pulse_range = SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US;
    uint32_t pulse_us = SERVO_MIN_PULSE_US + ((uint32_t)angle_x10 * pulse_range) / 1800u;
    
    // Generate PWM pulse
    GPIO_PinWrite(SERVO_GPIO, SERVO_PIN, 1u);
    SDK_DelayAtLeastUs(pulse_us, cpu_freq);
    GPIO_PinWrite(SERVO_GPIO, SERVO_PIN, 0u);
    
    return pulse_us;
}
```

**Clever Game Design**: The servo doesn't just track the ball—it also represents the countdown timer! This creates a physical "tension meter" as the game progresses.

**Servo State Machine**:
- `SERVO_OFF`: Menu/Game Over (motor de-energizes, saves power)
- `SERVO_TRACKING`: Running (active position updates)
- `SERVO_HOLD`: Paused (freezes at current position)

---

## Rendering Pipeline

### Framebuffer: RAM-Based Double Buffering

Games need flicker-free rendering. The OLED update takes time (I2C communication), so if you draw directly to the display, you'd see partial updates.

**Solution**: Draw to RAM first, then send the complete frame to the display.

```c
#define FB_WIDTH   128
#define FB_HEIGHT  64
#define FB_SIZE    (FB_WIDTH * (FB_HEIGHT / 8))  // 1024 bytes

void fb_clear(uint8_t value) {
    // Clear entire framebuffer in RAM (sets all pixels black or white)
}

void fb_set_pixel(int x, int y) {
    // Set a single pixel to ON in the in-memory buffer
}

void fb_flush_to_oled(void) {
    // Send the entire 1024-byte buffer to the display via I2C
    // Entire frame appears instantly (no flicker)
}
```

**Pixel Memory Layout**: Pixels are packed 8 per byte (one bit = one pixel):
```
Row 0:  [byte 0 (pixels 0-7)][byte 1 (pixels 8-15)]...
Row 1:  [byte 128][byte 129]...
```

### Graphics Rendering: Double-Buffer Pattern

Each game frame:

```c
fb_clear(0x00);              // Clear RAM buffer

draw_top_border();           // Draw game elements into RAM buffer
draw_bottom_border();
draw_side_borders();
draw_paddle(leftPaddleX, leftPaddleTopY, PADDLE_H);
draw_paddle(rightPaddleX, rightPaddleTopY, PADDLE_H);
draw_ball(ballX, ballY);

fb_flush_to_oled();          // Send complete frame to display at once
```

**Benefits**:
1. **No flicker**: Display sees complete frames only
2. **Efficient**: I2C runs once per frame, not once per draw operation
3. **Decoupled**: Drawing code doesn't know about I2C timing

### Text Rendering: Direct OLED Writes

Menus (main menu, mode select, game over) write text directly to the OLED using the `oled.h` library—not via the framebuffer. This is okay because menus don't animate and don't need the framebuffer's performance:

```c
if (gameState == STATE_MENU_MAIN) {
    if (menuNeedsRender) {
        fillOLED(0x00);                              // Clear OLED directly
        writeString((char*) "PONG", false, 52, 2);  // Write text at position
        writeString((char*) "Press START", false, 22, 4);
        writeString((char*) "to select mode", false, 22, 5);
        menuNeedsRender = false;  // Only redraw when state changes
    }
}
```

**Optimization**: `menuNeedsRender` flag ensures the menu text is drawn once, not every frame (which would be slow and cause flicker).

---

## Advanced Techniques

### 1. Pointer-Based Physics Updates

Functions modify game state through pointers instead of returning values. This allows a single function to update multiple variables:

```c
void resetBallAfterPoint(
    int *ballX, int *ballY,    // Pointers allow direct modification
    int *vx, int *vy,
    bool serveToRight,
    int *servePauseFrames)
{
    if (serveToRight) {
        *ballX = BALL_SPAWN_LEFT_X;   // Dereference and modify
        *vx = 4;
    }
    // ... more modifications
}

// Called like this:
resetBallAfterPoint(&ballX, &ballY, &ballVelocityX, &ballVelocityY, true, &servePauseFrames);
```

**Why?** Cleaner than returning a struct with all these values. Standard pattern in C for "output parameters."

### 2. Fixed-Point Arithmetic for Servo

Servo angles use fixed-point math (integers only, scaled by 10):

```c
uint16_t angle_x10;  // e.g., 1125 represents 112.5°
uint16_t base_angle_x10 = (uint16_t)((timerSeconds * 1800u) / max_time);
                        // 1800 = 180° * 10 (fixed-point scale)
```

**Why?** No floating-point unit overhead. Integer arithmetic is faster and smaller.

### 3. Hysteresis Filtering

Potentiometers produce noisy ADC readings. To prevent paddle flicker:

```c
static int32_t lastPotRaw = -1;

if (lastPotRaw < 0 || (potRaw - lastPotRaw > 50) || (lastPotRaw - potRaw > 50)) {
    lastPotRaw = potRaw;  // Only update if change > 50 units
    leftPaddleTopY = mapPotentiometerTopaddleY((uint32_t) potRaw);
}
```

Only accept a new ADC value if it differs by more than 50 units (~1.2% of range). Small fluctuations are ignored.

### 4. EMA (Exponential Moving Average) Smoothing

Distance sensor readings are smoothed to reduce noise:

```c
smoothedCm = 0.35f * cm + 0.65f * smoothedCm;
```

This formula weights:
- 35% of the new reading
- 65% of the previous smoothed value

Result: Gradual response to changes, noise rejection:
- Real change: gradually tracked
- Noise spike: mostly ignored

### 5. Modulo Operator for Periodic Behavior

AI reaction and buzzer frames use modulo:

```c
if ((frameCount % AI_REACT_EVERY_N_FRAMES) != 0)
    return;  // Only react every N frames

if ((debugCounter % 10) == 0)
    PRINTF(...);  // Print debug info every 10 frames
```

`frameCount % 5 == 0` is true every 5 frames (0, 5, 10, 15, ...).

### 6. Bitwise Operations for Flagging

Menu/mode select redraw optimization:

```c
static bool menuNeedsRender = true;

if (gameState == STATE_MENU_MAIN) {
    if (menuNeedsRender) {
        // Expensive rendering
        menuNeedsRender = false;
    }
}
```

Simple boolean flag avoids re-rendering the same frame multiple times.

### 7. Static Variables for Persistent State

Input debouncing uses static variables to retain state across function calls:

```c
static bool btnPrev = true;
static uint32_t btnDebounce = 0;

// This code runs in a loop (main)
// Each iteration, btnPrev and btnDebounce retain their previous values
// (They don't reset like local variables would)
```

**Benefit**: Cleaner than using a struct; state is scoped to the code that uses it.

### 8. Calibrated Frame Timing for Physics Simulation

The 1.5 seconds of CPU time = 1 real second calibration is an example of **empirical tuning**:

```c
while (timerAccumUs >= 1500000u && timerSeconds > 0u) {  // 1.5 sections of work = 1 real sec
    timerAccumUs -= 1500000u;
    timerSeconds--;
}
```

This was discovered by:
1. Measuring how long a frame actually takes (DWT cycle counter)
2. Noticing systematic overhead from I2C, sensors, debug output
3. Scaling the timer accordingly

**Lesson**: Real-time embedded code needs calibration for accuracy, not just theoretical calculations.

---

## Hardware Integration Notes

### Why Level Shifter is Needed

The MCXN947 microcontroller operates at 3.3V logic levels. Some peripherals (like the distance sensor's echo pin) might operate at different voltages. A **level shifter** converts between voltage domains:

- **3.3V → 5V**: Boost microcontroller output for 5V sensors
- **5V → 3.3V**: Protect microcontroller inputs from 5V signals

In this project, check the specific voltage rails of:
- HC-SR04 distance sensor (typically 5V)
- Potentiometers (rail voltage)
- Servo motor (typically 5V)

The level shifter should be on the distance sensor's ECHO pin (input to MCU) if the sensor operates at 5V.

### I2C Bus Sharing

The OLED and LCD share a single I2C bus. This requires:
1. Different I2C addresses for each device
2. Sequential reads/writes (no simultaneous communication)
3. Proper timeout handling

The `i2c_bus.c` module handles this coordination.

---

## Code Organization Patterns

### Init Functions
Every module has an `Init()` or `init()` function called at startup:
- `Pot_Init()`, `Buzzer_Init()`, `Servo_Init()`, etc.
- Sets up hardware, enables clocks, initializes state

### Update/Tick Functions
Each frame, functions update their state and perform I/O:
- `Buzzer_Update()`: Decrement frame counter, control GPIO
- `Servo_Update()`: Calculate new angle, generate PWM pulse

### Modular Read Functions
Sensor/input code exposed through simple read functions:
- `Pot_ReadRaw()`: Returns ADC value or -1 on error
- `HCSR04_ReadCm()`: Returns true if valid, writes distance to pointer

---

## Example: Explaining the Servo Code to Your Mentor

**Setup**: "The servo motor tracks the countdown timer's remaining seconds, using PWM."

**Basic Operation**:
1. Timer counts from 30 to 0 seconds
2. Servo angle maps linearly: 30 sec = 180°, 0 sec = 0°
3. Servo angle generated via PWM: width of HIGH pulse determines position

**Advanced Feature - Time Pressure**:
4. In the last 10 seconds, servo "ticks" (oscillates ±15°) to create tension
5. Ticking accelerates as time runs out (makes the game feel urgent)
6. When paused, servo freezes at current angle (SERVO_HOLD state)

**Code Flow**:
- `Servo_Update()` called once per frame with current timer seconds
- Converts seconds to angle using fixed-point math (no floating-point)
- Generates precise 20ms PWM pulse by bit-banging GPIO:
  - Assert HIGH, delay for pulse_width microseconds
  - Assert LOW for remaining 20ms
- Each frame's pulse updates servo position

**Real-Time Consideration**:
- Servo update **blocks** for its pulse duration (~1-2ms)
- But it's the very last thing in the frame, so there's time
- Overall frame still completes in 20ms because main loop did lighter work earlier

---

## Summary Table: Key Components

| Component | Purpose | Update Rate | I/O Type | Key Feature |
|-----------|---------|-------------|----------|-------------|
| **Game Loop** | Main FSM, physics, collision | Every frame (50 Hz) | CPU | Real-time DWT timing |
| **Ball Physics** | Movement, wall bounce, scoring | Every frame | Computation | 3-zone paddle angle |
| **Paddles** | Player/AI positioning | Every frame | I/O depending on mode | Pointer-based updates |
| **Distance Sensor** | HC-SR04 ultrasonic input | Every 3 frames (60ms) | GPIO + timing | EMA smoothing |
| **Potentiometer** | ADC analog input | Every 1-2 frames | ADC | Hysteresis filtering |
| **AI** | Right paddle control (D-sensor mode) | Every 5 frames (slower) | Computation | LCG randomness |
| **Buzzer** | Sound feedback | Every frame update check | GPIO (digital) | Frame counter (non-blocking) |
| **Servo Motor** | Ball position indicator + timer | Every frame | GPIO PWM | Fixed-point math, pulse timing |
| **LCD** | Score display | When score changes | I2C | Lazy update (only redraw if needed) |
| **OLED** | Menu/game graphics | Menu: once; Game: every frame | I2C (framebuffer) | Double-buffering, static flag optimization |
| **Buttons** | User input (menu, pause, reset) | Every frame (debounced) | GPIO | Software debouncing with edge detection |
| **Mode Switch** | Toggle input for sensor mode | Every frame | GPIO | No debounce (toggle, not momentary) |

---

## Learning Checklist for Mentor Meeting

- [ ] **State Machine**: Can explain 5 states and transitions
- [ ] **Game Loop Timing**: Understand DWT cycle counter and 1.5x calibration
- [ ] **Ball Physics**: Describe 3-zone paddle angles and overlap detection
- [ ] **Distance Sensor**: Explain ultrasonic timing and EMA smoothing
- [ ] **Potentiometer**: Tell how ADC maps to paddle position with hysteresis
- [ ] **Debouncing**: Walk through the debounce algorithm and why it's needed
- [ ] **Servo Motor**: Describe PWM pulse timing and how angle maps to time
- [ ] **AI Algorithm**: Explain reaction delay, deadzone, and hesitation
- [ ] **Framebuffer**: Explain double-buffering and why it's important
- [ ] **Input/Output**: Identify which I/O is blocking vs non-blocking

---

## Additional Resources to Study

1. **PWM (Pulse Width Modulation)**: How servo position is set by pulse width
2. **Ultrasonic Measurement**: Speed of sound, time of flight calculation
3. **I2C Protocol**: How OLED and LCD communicate, addressing, timing
4. **ADC (Analog-to-Digital Converter)**: How potentiometer voltage → digital value
5. **GPIO Debouncing**: Theory behind switch bounce and filtering
6. **Fixed-Point Arithmetic**: Why integers are used instead of floats
7. **Finite State Machines (FSM)**: Software design pattern used in the game loop

---

**Document Version**: 1.0  
**Last Updated**: February 28, 2026  
**Project**: FRDM-MCXN947 PONG Game
