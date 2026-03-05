# PONG Game - Quick Reference Cheat Sheet

**Use this before your mentor meeting to quickly review key concepts.**

---

## State Machine (5 States)

```
MENU_MAIN → [START] → MODE_SELECT → [START] → RUNNING ↔ PAUSED
   ↑                                              ↓
   ├──────────────── [RESET] ←──────────────────┤
   ├─ GAME_OVER (3s auto-transition) ──────────┘
   │
   └─ Only state shown on menu/game-over screens
```

---

## Game Loop (50 Hz = 20ms per frame)

```c
while (1) {
    // 1. Measure real elapsed time using DWT cycle counter
    uint32_t loopStartCycle = DWT->CYCCNT;
    uint32_t elapsedUs = cyclesToUs(loopStartCycle - lastLoopCycle, cpu_freq);
    
    // 2. Update countdown timer (accumulates real time, not frame count)
    timerAccumUs += elapsedUs;
    if (timerAccumUs >= 1500000u) {  // 1.5x factor empirical calibration
        timerAccumUs -= 1500000u;
        timerSeconds--;
    }
    
    // 3. Read inputs (buttons, sensors)
    // 4. Update game physics (only if STATE_RUNNING)
    // 5. Update outputs (LCD, buzzer, servo)
    // 6. Render graphics (framebuffer → OLED)
    
    // 7. Sleep for remaining frame time
    uint32_t workUs = cyclesToUs(DWT->CYCCNT - loopStartCycle, cpu_freq);
    uint32_t remaining = FRAME_PERIOD_US - workUs;
    SDK_DelayAtLeastUs(remaining, cpu_freq);
}
```

---

## Input Systems at a Glance

### Buttons
- **Hardware**: GPIO (active low = pressed, pull-up)
- **Debounce**: Software state machine (n frames stable before accept)
- **Action**: Only on falling edge (1→0)

### Potentiometers
- **Hardware**: ADC (12-bit, 0-4095)
- **Noise**: Hysteresis filter (only update if change > 50 units)
- **Mapping**: ADC → normalized [0,1] → paddle Y position
- **Both Pots**: Left (every frame), Right (every 2 frames)

### Distance Sensor (HC-SR04)
- **Precision**: Microsecond timing via SysTick
- **Steps**: Send 10µs pulse → wait for echo → measure duration
- **Time-to-distance**: `dist = (time × 0.034 cm/µs) / 2`
- **Smoothing**: EMA 35% new + 65% old
- **Rate**: Every 3 frames (60ms) due to sensor speed

### Mode Switch
- **Function**: Toggle between Distance Sensor ↔ Potentiometer mode
- **No debounce**: Permanent toggle, not momentary

---

## Physics & Collision

### Ball Motion
```
Each frame:
  ballX += ballVelocityX  (typically ±4 px/frame)
  ballY += ballVelocityY  (typically -2, 0, or +2 px/frame)
```

### Wall Bounce (Elastic)
```
if (ballY <= 1 or ballY >= 61) {
    ballVelocityY = -ballVelocityY;  // Reverse
}
```

### Paddle Collision (3-Zone Angles)
```
Hit Zone          → Resulting Angle
├─ Top third      → VY = -2  (strong up)
├─ Middle third   → VY =  0  (flat)
└─ Bottom third   → VY = +2  (strong down)
```

### Overlap Detection
```c
bool verticalRangesOverlap(int topA, int bottomA, int topB, int bottomB) {
    return !(bottomA < topB || bottomB < topA);
    // True if ranges overlap vertically
}
```

---

## AI Paddle (Distance Sensor Mode)

```c
if (frameCount % AI_REACT_EVERY_N_FRAMES == 0) {  // Reaction delay
    if (rngNext() % 100 < AI_HESITATE_PERCENT) {    // Random hesitation
        return;  // Skip move
    }
    
    int diff = ballCenterY - paddleCenterY;
    if (diff > AI_DEADZONE_PIXELS) {
        paddleTopY++;  // Move down
    } else if (diff < -AI_DEADZONE_PIXELS) {
        paddleTopY--;  // Move up
    }
}
```

**Difficulty Tuning**:
- ↑ Reaction frames → Easier (slower AI)
- ↑ Hesitate % → Easier (dumber AI)
- ↓ Deadzone → Harder (more precise aiming)

---

## Debouncing (Button Press Flow)

```
Raw GPIO:  1  0  0  1  0  0  0  0  0  0  1  1
          ─────────┬─────────────────┬──────
Active:         bounce1           press
                                    ↓
Debounce:  1  1  1  1  1  0  0  0  0  0  1  1
Counter:   0  1  0  1  0  1  2  0  0  0  1  0

On transition (btnRaw ≠ btnPrev):
  ├─ Increment counter
  ├─ If counter ≥ DEBOUNCE_FRAMES:
  │  ├─ Accept new state
  │  └─ Fire action if falling edge (1→0)
  └─ Reset counter only when state is stable
```

**Example**: After 20ms of stable LOW, falling edge detected → button press fires.

---

## Servo Motor Control

```
Hardware: GPIO0_28, 20ms period (50 Hz)
Stroke:   1000µs → 0°,  1500µs → 90°,  2000µs → 180°

Code pattern:
  1. Convert game data (timer) → angle_x10 (fixed-point)
  2. angle_x10 → pulse_us using formula
  3. assert HIGH, delay(pulse_us), assert LOW
  4. Repeat every 20ms
```

**Game Twist**: Servo sweeps time remaining (180° at start → 0° at end).

---

## Output Non-Blocking Patterns

### LCD Display
```c
if (score changed) {
    lcd_show_score(scoreLeft, scoreRight);  // Only if needed
}
```

### OLED Framebuffer
```c
fb_clear(0x00);           // Clear RAM buffer
draw_paddle(...);         // Draw to buffer (RAM, not I2C)
draw_ball(...);
fb_flush_to_oled();      // One I2C write per frame
```

### Buzzer
```c
Buzzer_Beep(3);  // Request 3 frames of beeping

// In every frame update:
Buzzer_Update() {
    if (frames_remaining > 0) {
        frames_remaining--;
        GPIO = ON;
    } else {
        GPIO = OFF;
    }
}
```

### Servo
```c
servoState = (gameState == RUNNING) ? SERVO_TRACKING :
             (gameState == PAUSED)  ? SERVO_HOLD :
                                      SERVO_OFF;
Servo_Update(servoState, timerSeconds);  // Called last (takes ~1.5ms)
```

---

## Fixed-Point Arithmetic (No Floats!)

```c
uint16_t angle_x10;  // "x10" means multiply real value by 10

Examples:
  90.5° is stored as 905
  180.0° is stored as 1800
  0.1°  is stored as 1

Conversions:
  Real → Fixed:  (real_angle * 10) → uint16_t
  Fixed → Real:  (angle_x10 * 0.1) → float
```

**Why?** Embedded systems often avoid floating-point (slower, larger code).

---

## Mapping Formulas

### ADC → Paddle Y
```
t = potRaw / 4095.0          [normalize to 0.0-1.0]
y = (1.0 - t) * 43 + 1       [43 = yMax - yMin]
```
(Inverted so max ADC = top of screen)

### Distance → Paddle Y
```
t = (cm - 6.0) / (18.0 - 6.0)  [0.0=far, 1.0=near]
y = (1.0 - t) * 43 + 1
```
(Hand close → bottom, hand far → top)

### Timer → Servo Angle
```
angle_x10 = (timerSeconds * 1800) / 30
```
(30 seconds → 180°, 0 seconds → 0°)

---

## DWT Cycle Counter (Real Timing)

```c
CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;  // Enable trace
DWT->CYCCNT = 0u;
DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;              // Start counting

// In loop:
uint32_t start = DWT->CYCCNT;
// ... do something ...
uint32_t elapsed_cycles = DWT->CYCCNT - start;
uint32_t elapsed_us = (elapsed_cycles * 1000000) / cpu_freq_hz;
```

**Why?** I2C, sensor reads, and debug output cause variable loop timing.
The cycle counter measures actual CPU work, not frame count.

---

## EMA Smoothing Formula

```c
smoothed = 0.35 * new_value + 0.65 * smoothed;
```

- **0.35** (alpha) = how much new reading influences result
- **0.65** (1-alpha) = how much previous value is retained
- **Effect**: Noise rejected, real trends followed

**For heavier smoothing**, decrease alpha (e.g., 0.2).

---

## Hysteresis (Noise Rejection)

```c
if (lastRaw < 0 ||                    // Never read before
    (newRaw - lastRaw > THRESHOLD) || // Large jump up
    (lastRaw - newRaw > THRESHOLD)) { // Large jump down
    lastRaw = newRaw;  // Accept change
    updatePosition(newRaw);
}
```

Only accept changes larger than noise amplitude.

---

## Random Number Generation (AI Hesitation)

```c
static uint32_t rngState = 0x12345678u;
uint32_t rngNext() {
    rngState = (1103515245u * rngState + 12345u);
    return rngState;
}
```

Usage: `if (rngNext() % 100 < 25) { hesitate; }`

---

## Key Constants

```c
#define FRAME_PERIOD_US      20000u    // 20ms = 50 Hz
#define PADDLE_H             18u       // Paddle height
#define TIMER_TOTAL_SECONDS  30u       // Game duration
#define AI_REACT_EVERY_N_FRAMES  5     // AI moves every ~100ms
#define AI_HESITATE_PERCENT  25        // 25% chance to skip move
#define AI_DEADZONE_PIXELS   2         // Stop jitter within 2px
```

---

## Explaining to Your Mentor

### "How does the servo work?"
> It uses PWM. GPIO0_28 sends a HIGH pulse every 20ms. Pulse width (1000-2000µs) tells the servo the position (0-180°). We map the countdown timer to angle: 30sec=180°, 0sec=0°. The servo oscillates (ticks) faster as time runs out, creating tension.

### "How does debouncing work?"
> Software state machine: track raw GPIO and stable state separately. Only update stable when raw has been different for N consecutive frames. Only fire action on falling edge (1→0). This filters switch bounce (20-50ms electrical noise).

### "How do controls work?"
> Mode switch selects between two modes. Potentiometer mode: ADC reads paddle Y directly (hysteresis filtering prevents jitter). Distance mode: ultrasonic pulse times echo return, calculates distance via speed of sound, applies EMA smoothing. Both map to paddle Y using linear interpolation.

### "Why non-blocking I/O?"
> Only 20ms per frame. Blocking on any I/O (I2C, sensor reads) would make game stall. Solution: each module manages its own state. Buzzer has frame counter. Sensors cache values read between frames. LCD only updates when needed.

### "How's the timer accurate?"
> Frames vary in length (I2C ops block). DWT cycle counter measures actual CPU work time. We accumulate microseconds and divide by 1.5x empirical factor: 1.5 seconds of CPU work = 1 real second.

---

## Potential Mentor Questions & Answers

**Q: What happens if the user presses START while the game is running?**  
A: Button triggers pause (STATE_RUNNING → STATE_PAUSED). Pressing again resumes. Ball and AI freeze while paused.

**Q: Why read the distance sensor every 3 frames instead of every frame?**  
A: The sensor takes ~50ms per measurement and is blocking. Reading every 20ms (every frame) could waste CPU waiting. Every 3 frames = 60ms, matches sensor update rate, non-blocking via caching.

**Q: How does the game ensure 50 FPS consistency?**  
A: DWT cycle counter measures actual work time. After physics/drawing, sleep for remaining duration to hit 20ms total. This compensates for variable I2C and sensor delays.

**Q: What would happen if you removed hysteresis on the potentiometer?**  
A: Paddle would jitter (twitch) because ADC noise causes small reading changes every frame.

**Q: Can the AI be beaten?**  
A: Yes! It reacts every 5 frames (slow), hesitates 25% of the time (random stupidity), and has a deadzone (can't aim precisely). Humans can win by moving faster.

**Q: Why not use a neural network for AI?**  
A: Overkill for a simple game. Also, embedded systems have limited memory/CPU. The current AI is tunable (adjust reaction/hesitate/deadzone) and fun (not boring determinism).

**Q: What's the purpose of the servo in the game?**  
A: Tracks time visually. Servo sweeps 180° → 0° as timer counts down 30 → 0 seconds. Creates tension via ticking sound. Pure aesthetics (game would work without it).

**Q: If I change AI_DEADZONE_PIXELS from 2 to 0, what happens?**  
A: AI becomes twitchy—constantly moves even when ball is perfectly centered. Harder to predict, might feel unfair.

---

## Quick Debugging Tips

| Problem | Likely Cause | Check |
|---------|-------------|-------|
| Timer doesn't decrement | `wasRunningLastLoop` not set? | Verify STATE_RUNNING check |
| Paddle jitters | Hysteresis threshold too low | Increase threshold from 50 → 100 |
| Servo doesn't move | Pin configuration? | Verify GPIO0_28 setup, SERVO_MIN/MAX_PULSE |
| Button too sensitive (bounces) | Debounce frames too low | Increase BUTTON_DEBOUNCE_FRAMES |
| Ball moves erratically | DWT not initialized? | Check `initCycleCounter()` called |
| AI paddle unresponsive | Reaction frequency too high | Decrease AI_REACT_EVERY_N_FRAMES |
| Sensor readings noisy | Smoothing weak | Increase EMA alpha (0.35 → 0.50) |

---

## Code Reading Path (Understanding Order)

1. **main.c** - Start here: game loop, state machine, overall flow
2. **game_logic.c** - Collision detection and ball reset
3. **servo.c** - PWM generation, timer mapping
4. **distance_sensor.c** - Ultrasonic measurement, timing precision
5. **pot.c** / **buzzer.c** - Simple ADC/GPIO modules
6. **draw.c** / **framebuffer.c** - Graphics rendering
7. **motor control and other features** - Optional deep dive

---

**Last Review**: February 28, 2026  
**Good luck with your mentor meeting!**
