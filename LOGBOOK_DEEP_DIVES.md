# PONG Game - Deep Dives: Advanced Topics

## Quick Reference for Mentor Questions

This document provides detailed answers to likely mentor questions about the more complex systems.

---

## Q1: "How Does the Servo Code Work?"

### TL;DR
The servo motor is controlled via PWM (Pulse-Width Modulation). Every 20ms, we send a HIGH pulse on GPIO0_28. The **width** of that pulse (1000-2000 microseconds) tells the servo what angle to move to. The game uses this to create a visual "timer" that tracks remaining seconds by moving the servo's position based on how much time is left.

### Detailed Walkthrough

#### Servo Hardware Background
The SG90 servo (typical hobby servo) is a motor with:
- Built-in electronics that read a PWM signal
- Gears that move the motor to match the signal
- Mechanical feedback to hold position

**Control Protocol**:
- Receives a 50 Hz signal (20ms period)
- Pulse width determines position:
  - 1000 µs = 0° (full left)
  - 1500 µs = 90° (center)
  - 2000 µs = 180° (full right)

```
One 20ms period:
┌────────────────────────────────────────────┐  20 ms total
│← pulse →│←───── low time ─────→│
║1-2 ms   │  rest of period      │
(SERVO_MIN/MAX_PULSE_US)
```

#### Code Structure

**Initialization** (called once at startup):
```c
void Servo_Init(void)
{
    // Set GPIO0_28 as digital output, initially LOW
    gpio_pin_config_t servo_config = {kGPIO_DigitalOutput, 0};
    GPIO_PinInit(SERVO_GPIO, SERVO_PIN, &servo_config);
    GPIO_PinWrite(SERVO_GPIO, SERVO_PIN, 0u);
    
    // Start at 180° (full right, representing timer at start)
    s_last_angle_x10 = 1800u;  // 1800 = 180.0° in fixed-point
}
```

**Main Update** (called once per frame):
```c
uint32_t Servo_Update(ServoState state, uint32_t timerSeconds)
{
    uint32_t cpu_freq = CLOCK_GetFreq(kCLOCK_CoreSysClk);  // Get CPU speed in Hz
    
    // Determine the angle to move to
    uint16_t angle_x10;  // Angle in 0.1° units (fixed-point, integer only)
    
    if (state == SERVO_OFF) {
        // Menu/Game Over: keep pin LOW (no pulse = motor de-energizes)
        GPIO_PinWrite(SERVO_GPIO, SERVO_PIN, 0u);
        return 0u;
    }
    
    if (state == SERVO_TRACKING) {
        // Convert countdown timer to angle
        // 30 seconds → 180°, 0 seconds → 0°
        uint32_t max_time = 30u;
        if (timerSeconds > max_time) timerSeconds = max_time;
        uint16_t base_angle_x10 = (uint16_t)((timerSeconds * 1800u) / max_time);
        
        // Calculate ticking (oscillation for time pressure effect)
        static uint32_t tick_counter = 0u;
        tick_counter++;
        
        // Determined tick speed based on time remaining
        uint32_t tick_interval = 10u;  // Slow at first
        if (timerSeconds < 10u) {
            tick_interval = 1u;  // Fast in last 10 seconds
        } else if (timerSeconds < 20u) {
            tick_interval = 5u;  // Medium in 10-20 seconds
        }
        
        // Calculate oscillation: toggle between ±15°
        uint16_t tick_offset = ((tick_counter / tick_interval) % 2u) ? 150u : 0u;
        
        // Apply oscillation (but only when it won't go negative)
        if (timerSeconds < 28u && base_angle_x10 >= tick_offset) {
            angle_x10 = base_angle_x10 - tick_offset;  // Subtract (move left)
        } else {
            angle_x10 = base_angle_x10 + tick_offset;  // Add (move right)
        }
        
        s_last_angle_x10 = angle_x10;  // Save for SERVO_HOLD state
    }
    else {  // SERVO_HOLD (game paused)
        // Freeze at the angle we were at when pause happened
        angle_x10 = s_last_angle_x10;
    }
    
    // Convert angle to pulse width
    // Formula: pulse_us = 1000 + (angle * 1000 / 180)
    uint32_t pulse_range = SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US;  // 1000 µs
    uint32_t pulse_us = SERVO_MIN_PULSE_US +     // Start at 1000 µs
                        ((uint32_t)angle_x10 * pulse_range) / 1800u;  // Add scaled angle
    
    // Generate the PWM pulse
    // 1. Assert pin HIGH
    GPIO_PinWrite(SERVO_GPIO, SERVO_PIN, 1u);
    
    // 2. Wait for the specified pulse width
    SDK_DelayAtLeastUs(pulse_us, cpu_freq);
    
    // 3. Assert pin LOW
    GPIO_PinWrite(SERVO_GPIO, SERVO_PIN, 0u);
    
    return pulse_us;  // Tell main loop how much time we used
}
```

#### The Game Design: Servo as Timer Visualizer

This is clever:
- Players see the countdown **visually** via servo sweeping from right (180°) to left (0°)
- They don't need to read the LCD; they can watch the servo
- Time pressure is emphasized by faster ticking as seconds run out

**Timeline**:
```
Start (30 sec)        20 sec mark     10 sec mark       End (0 sec)
  ↓                      ↓               ↓                 ↓
  └━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━┳━━━━━━┳━━━━━━┳━━━━━━━┛
                  slow ticking   medium ticking   FAST TICKING!
                  (every 10f)    (every 5f)       (every frame)
```

#### Fixed-Point Arithmetic: Why Not Floats?

Angle uses `uint16_t` with scale of 10:
```c
uint16_t angle_x10 = 1125;  // Represents 112.5°
                            // (multiply by 0.1 to get real angle)
```

**Advantages**:
1. No floating-point math library needed (saves code/memory)
2. Faster computation (integer multiplies vs floating-point)
3. Deterministic (no rounding surprises across different systems)

**Formula Example**:
```
timerSeconds = 15 (out of 30)
base_angle_x10 = (15 * 1800) / 30 = 27000 / 30 = 900
Actual angle = 900 * 0.1 = 90° (perfect center!)
```

---

## Q2: "How Does Debouncing Work? Why Do We Need It?"

### The Problem

Real switches don't instantly connect/disconnect. When pressed, the internal contacts **bounce** rapidly between connected and disconnected for 20-50ms:

```
Ideal digital transition:    Real switch behavior:
                           ─────────┐
    ──┐                     ──┐──┐ ┌─┘
      │ (clean & instant)      ├─┘ └─ (many transitions)
      └──────────────          └╌╌╌(bouncing zone ~20-50ms)
```

A naive debounce-less function:
```c
bool buttonPressed = (GPIO_PinRead(BUTTON_GPIO, BUTTON_PIN) == 0);
// This runs in a loop...
// During the 20ms bounce window, it might read:
// 0, 0, 1, 1, 0, 0, 1, 0, 0  ← looks like 4 separate presses!
```

### The Solution: Synchronous Sample & Hold

Only accept a new button state if the raw value has been **stable** for N consecutive frames.

```c
// These are STATIC variables (state persists across function calls)
static bool btnPrev = true;        // Last accepted stable state
static uint32_t btnDebounce = 0;   // Counter for unstable state duration

// Called every loop iteration (every 20ms)
bool btnRaw = (bool) GPIO_PinRead(BUTTON_GPIO, BUTTON_PIN);  // 1=released, 0=pressed

if (btnRaw != btnPrev) {
    // Raw value differs from stable value
    btnDebounce++;
    
    if (btnDebounce >= BUTTON_DEBOUNCE_FRAMES) {  // 1 frame = 20ms
        // Raw value has been different for N frames—accept it as new stable value
        btnPrev = btnRaw;
        btnDebounce = 0;
        
        if (!btnRaw) {  // Only act on falling edge (1→0 transition)
            // BUTTON PRESS DETECTED—handle it
        }
    }
} else {
    // Raw value equals stable value; reset bounce counter
    btnDebounce = 0;
}
```

### Walkthrough: Real Button Press

```
Frame:    1    2    3    4    5    6    7    8    ...
────────────────────────────────────────────────────
Raw:      1    0    0    1    0    0    0    0
Stable:   1    1    1    1    1    0    0    0

btnDebounce: 0 → 1 → 0 (reset) → 1 → 0 → 1 → 2 → 0

At frame 5:
- btnRaw = 0, differs from btnPrev = 1
- btnDebounce = 1 < BUTTON_DEBOUNCE_FRAMES (1)
- btnDebounce++ → 2
- Since 2 >= 1, we accept the new value
- btnPrev = 0
- Falling edge detected! Execute button press action
- Reset btnDebounce = 0

(Note: BUTTON_DEBOUNCE_FRAMES=1 means "stable for 1 frame = 20ms", which with
the hardware RC filter is sufficient; for purely software debouncing, often use 3-5)
```

### Falling Edge Detection: Why Only React Once?

The code checks `if (!btnRaw)` only after confirming a state transition:

```c
if (btnDebounce >= BUTTON_DEBOUNCE_FRAMES) {
    btnPrev = btnRaw;      // Accept new state
    
    if (!btnRaw) {         // Only when transitioning to pressed state
        // ONE-TIME PRESS ACTION
    }
}
```

Two benefits:
1. **Single action per press**: Without this, the action would fire every frame while the button is held
2. **Clean semantics**: Only care about the moment of pressing, not holding

### At This Project: Hardware RC Filter Helps

The board has a passive RC filter on the button (resistor + capacitor). This provides **hardware** debouncing:

```
Button ──┬──[ R ]──┬─→ GPIO input
         └─[ C ]──┴─ (GND)

Effect: RC network filters high-frequency bounce,
        leaving only the underlying press signal
```

With hardware filtering, the software debounce only needs 1 frame instead of 5-10.

---

## Q3: "How Do the Controls Work (Potentiometer vs. Distance Sensor)?"

### Mode Switch Selection

A hardware toggle switch (GPIO0_26) selects the control method:

```c
typedef enum {
    MODE_DISTANCE_SENSOR,  // HC-SR04 input, right paddle = AI
    MODE_POTENTIOMETER     // 2x potentiometers, no AI
} GameMode;

// Read every frame (no debounce needed—it's a toggle, not momentary)
GameMode switchMode = GPIO_PinRead(SWITCH_GPIO, SWITCH_PIN) ?
                      MODE_DISTANCE_SENSOR : MODE_POTENTIOMETER;
```

### Potentiometer Mode (Analog Input via ADC)

#### Overview
- **Left paddle**: ADC1_A0 (potentiometer 1)
- **Right paddle**: ADC1_A3 (potentiometer 2)
- Raw 12-bit values (0-4095) map to paddle Y position (1-44 pixels)

#### Reading
```c
int32_t potRaw = Pot_ReadRaw();  // Returns -1 on error, else 0-4095
if (potRaw >= 0) {
    // Use the value
}
```

The ADC hardware samples the analog voltage and returns a digital value.

#### Mapping: ADC Value → Paddle Position
```c
static int mapPotentiometerTopaddleY(uint32_t potValue) {
    // Normalize to 0.0-1.0
    float t = (float) potValue / 4095.0f;
    //   potValue=0      → t=0.0
    //   potValue=2047   → t≈0.5
    //   potValue=4095   → t=1.0
    
    const int yMin = 1;
    const int yMax = 62 - PADDLE_H;  // 44 (screen height - paddle height)
    
    // Invert: high ADC = high on screen (counterintuitive, but matches physical orientation)
    int y = (int) ((1.0f - t) * (yMax - yMin) + yMin);
    //   potValue=0    → y = 44 (bottom of screen)
    //   potValue=2047 → y ≈ 22 (middle)
    //   potValue=4095 → y = 1 (top of screen)
    
    return y;
}
```

**Linear Interpolation Explanation**:
```
Formula: y = (1.0f - t) * (yMax - yMin) + yMin

yMax - yMin = 44 - 1 = 43 (total range)

When t=0 (pot at min):
  y = 1.0 * 43 + 1 = 44  ✓ bottom

When t=0.5 (pot at middle):
  y = 0.5 * 43 + 1 = 22.5 ≈ 22  ✓ middle

When t=1.0 (pot at max):
  y = 0.0 * 43 + 1 = 1   ✓ top
```

#### Hysteresis: Fighting ADC Noise
Potentiometers produce noisy readings. Without filtering, the paddle flickers:

```c
static int32_t lastPotRaw = -1;

if (lastPotRaw < 0 || (potRaw - lastPotRaw > 50) || (lastPotRaw - potRaw > 50)) {
    // Only update if:
    // 1. Haven't read before (lastPotRaw < 0)
    // 2. Increased by > 50 units
    // 3. Decreased by > 50 units
    lastPotRaw = potRaw;  // Accept new reading
    leftPaddleTopY = mapPotentiometerTopaddleY((uint32_t) potRaw);
}
```

**Example**:
```
Before: 2000
New ADC: 2005 → Difference = 5 < 50 → Ignore
New ADC: 2060 → Difference = 60 ≥ 50 → Accept! Update paddle
```

Small fluctuations (noise) don't cause movement. Only significant changes move the paddle.

### Distance Sensor Mode (Ultrasonic Input)

#### Principle
The HC-SR04 is an **ultrasonic rangefinder**:
1. Send a sound pulse
2. Measure the time until echo returns
3. Calculate distance from time (sound is slow)

#### Sound Physics
- Speed of sound ≈ 343 m/s = 0.034 cm/µs (centimeters per microsecond)
- Distance = (echo_time × speed_of_sound) / 2
  - Divide by 2 because sound travels to object and back

#### Single Measurement Sequence

```c
bool HCSR04_ReadCm(const hcsr04_t *dev, float *outCm) {
    // 1. Wait for echo to be settled (LOW) from any previous measurement
    if (!wait_for_echo_level(dev, 0U, 2000U)) {
        return false;  // Timeout
    }
    
    // 2. Send 10µs trigger pulse
    GPIO_PinWrite(dev->trigGpio, dev->trigPin, 0u);
    SDK_DelayAtLeastUs(2u, SystemCoreClock);   // Ensure LOW for >1µs
    GPIO_PinWrite(dev->trigGpio, dev->trigPin, 1u);
    SDK_DelayAtLeastUs(10u, SystemCoreClock);  // Hold HIGH for 10µs
    GPIO_PinWrite(dev->trigGpio, dev->trigPin, 0u);  // Pulse complete
    
    // 3. Wait for ECHO pin to go HIGH (sensor starts transmitting sound)
    if (!wait_for_echo_level(dev, 1U, HCSR04_ECHO_RISE_TIMEOUT_US)) {
        return false;  // Sensor didn't respond
    }
    
    // 4. Record the start time
    uint32_t startTick = SysTick->VAL;  // Hardware timer snapshot
    
    // 5. Wait for ECHO pin to go LOW (sound has returned)
    if (!wait_for_echo_level(dev, 0U, HCSR04_ECHO_FALL_TIMEOUT_US)) {
        return false;  // Echo didn't return
    }
    
    // 6. Calculate echo duration
    uint32_t elapsedTicks = SysTick_ElapsedTicks(startTick);
    float duration_us = ((float)elapsedTicks * 1000000.0f) / (float)SystemCoreClock;
    
    // 7. Convert time to distance
    float dist_cm = (duration_us * 0.034f) / 2.0f;
    
    // 8. Validate and return
    if (dist_cm < 2.0f || dist_cm > 400.0f) {
        return false;  // Out of range
    }
    
    *outCm = dist_cm;
    return true;
}
```

**Timeline Diagram**:
```
Time:     0      10µs         Echo time (variable!)
                                         ↓
TRIG:     LOW──┐                        
             └─ HIGH (10µs) ─ LOW
                       ↑ trigger sent

ECHO:               LOW─────┐
                           └─ HIGH (300-18000µs) ─ LOW
                             ↑ sound leaves sensor, object reflection returns
```

#### Reading Frequency & Smoothing
The sensor is slow (takes 60ms per measurement), so we read every 3 frames:

```c
static int sensorFrameCounter = 0;
sensorFrameCounter++;

if (sensorFrameCounter >= 3) {  // Read every 3 frames = ~60ms
    sensorFrameCounter = 0;
    
    float cm;
    static float smoothedCm = 12.0f;  // Initial value: middle of valid range
    
    if (HCSR04_ReadCm(&sensor, &cm)) {
        // Clamp to valid sensor range
        if (cm < 6.0f) cm = 6.0f;
        if (cm > 18.0f) cm = 18.0f;
        
        // Apply EMA smoothing: 35% new, 65% history
        smoothedCm = 0.35f * cm + 0.65f * smoothedCm;
        
        // Convert to paddle position
        leftPaddleTopY = mapDistanceCmToPaddleY(smoothedCm);
    }
}
```

#### Distance → Paddle Position Mapping
```c
static int mapDistanceCmToPaddleY(float cm) {
    const float cmNear = 6.0f;   // Hand close to sensor = bottom of screen
    const float cmFar = 18.0f;   // Hand far from sensor = top of screen
    
    // Clamp
    if (cm < cmNear) cm = cmNear;
    if (cm > cmFar) cm = cmFar;
    
    // Normalize: 0.0 = far, 1.0 = near
    float t = (cm - cmNear) / (cmFar - cmNear);
    
    const int yMin = 1;
    const int yMax = 62 - PADDLE_H;
    
    // Invert t so that small distance (hand close) = high Y (bottom)
    int y = (int) ((1.0f - t) * (yMax - yMin) + yMin);
    
    return y;
}
```

#### EMA (Exponential Moving Average) Smoothing
```
smoothedCm = 0.35 * cm + 0.65 * smoothedCm
           = 35% new data + 65% historical data
```

**Effects**:
- Real movement: tracked gradually (smoother than jumpy raw readings)
- Noise spikes: heavily filtered out (one outlier doesn't affect paddle much)

**Example**:
```
Raw readings:  10, 10, 10, 15, 10, 10, 10
Smoothed:      10, 10, 10, 11.75, 11.19, 10.91, 10.60

The "15" spike is diluted because it's only 35% of the calculation.
```

#### Why AI Only in Sensor Mode?
- **Distance Sensor Mode**: Left paddle = human (sensor input), right paddle = AI
- **Potentiometer Mode**: Both paddles = humans (two potentiometers)

If both were AI in potentiometer mode, or both were humans in sensor mode, the game wouldn't make sense.

---

## Q4: "How Does the AI Paddle Work?"

### Overview
The right paddle in **Distance Sensor Mode** is controlled by AI. It's not a complex neural network—just a simple feedback algorithm with controllable difficulty.

### Core Algorithm

```c
static void updateAiPaddle(int *paddleTopY, int ballCenterY, int frameCount) {
    // 1. REACTION FREQUENCY: only "think" every N frames
    if ((frameCount % AI_REACT_EVERY_N_FRAMES) != 0) {
        return;  // No update this frame
    }
    
    // 2. HESITATION: randomly decide to do nothing
    if ((rngNext() % 100u) < AI_HESITATE_PERCENT) {
        return;  // AI "freezes" momentarily
    }
    
    // 3. CALCULATE: paddle center vs ball center
    int paddleCenterY = *paddleTopY + (PADDLE_H / 2);
    int diff = ballCenterY - paddleCenterY;
    //   diff > 0 : ball is below paddle center
    //   diff < 0 : ball is above paddle center
    
    // 4. DEADZONE: ignore small differences (keeps AI from twitching)
    if (diff > AI_DEADZONE_PIXELS) {
        (*paddleTopY)++;  // Ball is significantly below → move down
    } else if (diff < -AI_DEADZONE_PIXELS) {
        (*paddleTopY)--;  // Ball is significantly above → move up
    }
    // else: ball is close to center → don't move
    
    // 5. BOUNDS: keep paddle on screen
    *paddleTopY = clampValueToRange(*paddleTopY, 1, 62 - PADDLE_H);
}
```

### Controllable Difficulty Parameters

```c
#define AI_REACT_EVERY_N_FRAMES  5       // Reacts every 5 frames (100ms)
#define AI_DEADZONE_PIXELS       2       // Ignores ball if within 2 pixels of paddle center
#define AI_HESITATE_PERCENT      25      // 25% chance to do nothing
```

#### Effect of Each Parameter

**Reaction Frequency** (5 frames = 100ms):
```
Frame: 0  1  2  3  4  5  6  7  8  9  10  11 ...
Calc?: Y  N  N  N  N  Y  N  N  N  N  Y   N

With frameCount % 5:
  0 % 5 = 0 ✓ (calculate)
  1 % 5 = 1 ✗ (skip)
  2 % 5 = 2 ✗ (skip)
  ... 
  5 % 5 = 0 ✓ (calculate)
```

Slower reaction = human can win by moving faster. Change to 2 for harder AI, 10 for easier AI.

**Deadzone** (2 pixels):
- Without deadzone: AI constantly fidgets when ball is near
- With deadzone: AI smoothly tracks, holding position when ball is close enough
- Increase to 5 for easier AI (wider target zone)
- Decrease to 1 for harder AI (precise aiming)

**Hesitation** (25% chance):
- Every time AI calculates, it has a 25% chance to just skip the move
- Creates random moments of "AI stupidity"
- This is FUN—deterministic AI is boring and frustrating
- Increase to 50% for easier AI (more stupidity)
- Decrease to 5% for harder AI (more consistent)

### Random Number Generation

```c
static uint32_t rngState = 0x12345678u;  // Start with a seed

static uint32_t rngNext(void) {
    rngState = (1103515245u * rngState + 12345u);  // Linear Congruential Generator
    return rngState;
}

// Usage:
if ((rngNext() % 100u) < AI_HESITATE_PERCENT) {  // 25% chance
    return;
}
```

**Why this generator?**
- Fast: one multiply, one add
- Good enough for game AI (not cryptographically random, but fine for games)
- Standard formula (used in many systems)
- Deterministic (same seed = same sequence, useful for debugging)

---

## Q5: "How Does the Timer System Work?"

### The Challenge
Microcontrollers don't have accurate timers by default. The game loop spends variable time on:
- I2C communication (OLED/LCD updates): unpredictable delays
- Sensor reads (HC-SR04): taken every 3 frames, takes time
- GPIO operations: variable with interrupt priority

If we naively did `if (frameCount == 50) { timerSeconds--; }` (assuming 50 frames = 1 second), the timer would drift.

### Solution: Hardware Cycle Counter + Calibration

#### Step 1: Enable the DWT Cycle Counter
DWT (Data Watchpoint and Trace) is a hardware unit in Cortex-M33 CPUs that counts CPU clock cycles:

```c
static void initCycleCounter(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;  // Enable trace unit
    
    // Unlock DWT on devices that protect it
#ifdef DWT_LAR
    DWT->LAR = 0xC5ACCE55u;
#endif
    
    DWT->CYCCNT = 0u;  // Reset counter
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;  // Enable cycle counter
}
```

#### Step 2: Measure Elapsed Time Each Frame

```c
uint32_t loopStartCycle = DWT->CYCCNT;  // Snapshot at frame start
uint32_t elapsedUs = cyclesToUs(loopStartCycle - lastLoopCycle, cpu_freq);
lastLoopCycle = loopStartCycle;
```

Convert cycles to microseconds:
```c
static uint32_t cyclesToUs(uint32_t cycles, uint32_t cpuFreqHz) {
    return (uint32_t) (((uint64_t) cycles * 1000000ULL) / (uint64_t) cpuFreqHz);
}

// Example:
// cycles = 600000, cpuFreqHz = 100MHz (100,000,000 Hz)
// us = (600000 * 1000000) / 100000000 = 6000 microseconds
```

#### Step 3: Accumulate Real Time
```c
if (wasRunningLastLoop && timerSeconds > 0u) {
    timerAccumUs += elapsedUs;  // Add real elapsed time
    
    while (timerAccumUs >= 1500000u && timerSeconds > 0u) {
        timerAccumUs -= 1500000u;  // Subtract 1.5 seconds of accumulated work time
        timerSeconds--;  // Decrement display timer
    }
}
```

**The 1.5x factor**: Through empirical measurement, the main loop takes 1.5 seconds of **CPU work time** for every 1 second of **real elapsed time**. This is because:
- I2C operations block
- Sensor reads block
- Debug output blocks

So when the cycle counter shows 1.5 million microseconds of work, only 1 real second has passed.

### State for Accuracy

```c
uint32_t timerSeconds = 30;       // Countdown display (30 to 0)
uint32_t timerAccumUs = 0u;       // Accumulator (microseconds)
uint32_t lastLoopCycle = DWT->CYCCNT;  // Track previous timestamp
```

### Timer Update Only When Running

```c
bool wasRunningLastLoop = (gameState == STATE_RUNNING);

// In next frame:
if (wasRunningLastLoop && timerSeconds > 0u) {
    // Only accumulate if we were actually playing
}

// At end of loop:
wasRunningLastLoop = (gameState == STATE_RUNNING);
```

This ensures:
- **Paused**: accumulator doesn't increment (timer frozen)
- **Menu**: accumulator doesn't increment
- **Game Over**: timer doesn't tick below 0

---

## Q6: "How Do Input/Output Interact?"

### The Architecture: Non-Blocking Concurrency

The game loop is **cooperative multitasking** on a single-threaded CPU:
- One task can't block waiting for another
- Everything must update within the 20ms frame budget

```
Main Loop (every 20ms)
├─ Input Reading
│  ├─ Buttons (GPIO, instant)
│  ├─ Potentiometers (ADC, few µs)
│  └─ Distance Sensor (takes time, so sampled every 3-60ms, stored, read every frame)
├─ Game Physics (computation, no I/O)
├─ Output Updates (non-blocking)
│  ├─ LCD (only if score changed)
│  ├─ OLED (rendered entire frame via framebuffer)
│  ├─ Buzzer (decrements counter, non-blocking)
│  └─ Servo (generates PWM, takes ~1-2ms but scheduled last)
└─ Frame Timing (sleep for remaining time)
```

### Non-Blocking Pattern: Buzzer Example

Naïve (BLOCKING):
```c
Buzzer_On();
SDK_DelayAtLeastUs(60000u);  // Blocks entire game for 60ms!
Buzzer_Off();
```

Smart (NON-BLOCKING):
```c
// In input handler
if (scoreChanged) {
    Buzzer_Beep(3);  // "Start beeping for 3 frames"
}

// Every frame in main loop
Buzzer_Update() {
    if (s_buzzerFramesRemaining > 0) {
        s_buzzerFramesRemaining--;
        GPIO_PinWrite(BUZZER_GPIO, BUZZER_PIN, 1u);  // ON
    } else {
        GPIO_PinWrite(BUZZER_GPIO, BUZZER_PIN, 0u);  // OFF
    }
}
```

**Benefits**:
- Game doesn't stall waiting for sound
- Multiple things can happen in parallel (beep while physics runs)
- Consistent 50 FPS

### Input Sampling Strategy

| Input | Update Rate | Blocking | Strategy |
|-------|------------|----------|----------|
| Button | Every frame (debounced) | No | Raw GPIO read, debounce logic |
| Mode Switch | Every frame | No | Raw GPIO read, no debounce |
| Potentiometer | Every 1-2 frames | No | ADC read (few µs) |
| Distance Sensor | Every 3 frames | Partially* | Sensor read every 3 frames, blocking on measurement |

*Distance sensor read is actually semi-blocking (takes ~50ms), but we mitigate by:
1. Only reading every 3 frames
2. Storing result and using cached value for rendering
3. Not waiting for sensor in the same frame

### Output Scheduling Within 20ms Budget

```
Frame Duration = 20,000 µs

0 µs     |Start frame
         ├─ Input reading (< 100 µs)
         ├─ Physics calculation (< 500 µs)
         ├─ LCD update if needed (< 5,000 µs if required)
         ├─ OLED framebuffer update via I2C (~8,000 µs)
         ├─ Buzzer update (< 1 µs)
15,000 µs├─ Servo PWM (~1500 µs)
         ...
18,000 µs├─ Sleep for remaining time (~2000 µs)
20,000 µs|Frame end, loop repeats
```

(Servo is scheduled LAST because it's the most predictable—we know exactly how long it takes.)

---

## Summary: All Pieces Working Together

1. **Real-time kernel**: Every frame is 20ms—DWT cycle counter keeps score/timer accurate
2. **State machine**: Determines what should happen (menu, running, paused, game over)
3. **Non-blocking I/O**: All devices update without stalling the main loop
4. **Sensor fusion**: Distance sensor (smoothed) and potentiometers provide input flexibility
5. **Game logic**: Simple physics, deterministic collisions, but with emergent depth (3-zone paddle angles)
6. **Output variety**: LCD (discrete), OLED (graphics), buzzer (audio), servo (mechanical) all synchronized
7. **AI opponent**: Simple reactions made fun through reaction delays, deadzone, and randomness

---

**Document Version**: 1.0
**Last Updated**: February 28, 2026
