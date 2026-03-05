#include "distance_sensor.h"
#include "fsl_gpio.h"
#include "fsl_device_registers.h"
#include "clock_config.h"
#include "fsl_common.h"
#include "game_config.h"

/* --- Low-latency timing parameters --- */
#define HCSR04_ECHO_RISE_TIMEOUT_US   (30000U)  // Wait for echo to start
#define HCSR04_ECHO_FALL_TIMEOUT_US   (30000U)  // Wait for echo to end
#define HCSR04_DIST_MIN_CM            (2.0f)    // Minimum valid distance
#define HCSR04_DIST_MAX_CM            (400.0f)  // Maximum valid distance
#define SYSTICK_PERIOD_TICKS          (0x01000000UL)  // 24-bit max period

/* --- Calculate elapsed SysTick ticks since startTick --- */
static inline uint32_t SysTick_ElapsedTicks(uint32_t startTick)
{
    uint32_t nowTick = SysTick->VAL;
    
    // SysTick is decrementing
    if (startTick >= nowTick)
    {
        return (startTick - nowTick);
    }
    else
    {
        // Wraparound occurred
        return (startTick + (SYSTICK_PERIOD_TICKS - nowTick));
    }
}

/* --- Convert microseconds to SysTick ticks --- */
static inline uint32_t UsToTicks(uint32_t us)
{
    uint64_t ticks = ((uint64_t)SystemCoreClock * (uint64_t)us) / 1000000ULL;
    
    if (ticks >= (uint64_t)SYSTICK_PERIOD_TICKS)
    {
        ticks = SYSTICK_PERIOD_TICKS - 1U;
    }
    
    return (uint32_t)ticks;
}

/* --- Wait for ECHO pin to reach target level with timeout --- */
static bool wait_for_echo_level(const hcsr04_t *dev, uint32_t targetLevel, uint32_t timeoutUs)
{
    uint32_t startTick = SysTick->VAL;
    uint32_t timeoutTicks = UsToTicks(timeoutUs);
    
    while (GPIO_PinRead(dev->echoGpio, dev->echoPin) != targetLevel)
    {
        if (SysTick_ElapsedTicks(startTick) > timeoutTicks)
        {
            return false;  // Timeout
        }
    }
    
    return true;  // Success
}

/* --- Initialize the HC-SR04 distance sensor (optimized low-latency version) --- */
void HCSR04_Init(const hcsr04_t *dev)
{
    // Ensure SysTick is running as free-running timer
    if ((SysTick->CTRL & SysTick_CTRL_ENABLE_Msk) == 0u)
    {
        SysTick->LOAD = 0x00FFFFFFu;   // 24-bit reload
        SysTick->VAL  = 0u;
        SysTick->CTRL = SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_CLKSOURCE_Msk;
    }
    
    // Setup GPIO pins
    gpio_pin_config_t trig_config = {kGPIO_DigitalOutput, 0};
    gpio_pin_config_t echo_config = {kGPIO_DigitalInput, 0};
    
    GPIO_PinInit(dev->trigGpio, dev->trigPin, &trig_config);
    GPIO_PinInit(dev->echoGpio, dev->echoPin, &echo_config);
    
    // Ensure TRIG starts low
    GPIO_PinWrite(dev->trigGpio, dev->trigPin, 0u);
}

/* --- Read distance from HC-SR04 in cm (low-latency optimized) --- */
bool HCSR04_ReadCm(const hcsr04_t *dev, float *outCm)
{
    uint32_t startTick, elapsedTicks;
    
    // Ensure echo is settled from previous measurement
    if (!wait_for_echo_level(dev, 0U, 2000U))
    {
        return false;
    }
    
    // Send 10µs trigger pulse
    GPIO_PinWrite(dev->trigGpio, dev->trigPin, 0u);
    SDK_DelayAtLeastUs(2u, SystemCoreClock);
    GPIO_PinWrite(dev->trigGpio, dev->trigPin, 1u);
    SDK_DelayAtLeastUs(10u, SystemCoreClock);
    GPIO_PinWrite(dev->trigGpio, dev->trigPin, 0u);
    
    // Wait for echo to go high (start of pulse)
    if (!wait_for_echo_level(dev, 1U, HCSR04_ECHO_RISE_TIMEOUT_US))
    {
        return false;
    }
    startTick = SysTick->VAL;
    
    // Wait for echo to go low (end of pulse)
    if (!wait_for_echo_level(dev, 0U, HCSR04_ECHO_FALL_TIMEOUT_US))
    {
        return false;
    }
    
    // Calculate echo pulse duration in microseconds
    elapsedTicks = SysTick_ElapsedTicks(startTick);
    float duration_us = ((float)elapsedTicks * 1000000.0f) / (float)SystemCoreClock;
    
    // Convert time to distance (speed of sound ~0.034 cm/µs, divide by 2 for round trip)
    float dist_cm = (duration_us * 0.034f) / 2.0f;
    
    // Filter out-of-range measurements
    if (dist_cm < HCSR04_DIST_MIN_CM || dist_cm > HCSR04_DIST_MAX_CM)
    {
        return false;
    }
    
    *outCm = dist_cm;
    return true;
}

//Convert measured hand distance (cm) into a paddle top-edge Y position
//Range: cmNear (6cm) maps to bottom of screen, cmFar (18cm) maps to top
int HCSR04_MapToPaddleY(float cm) {
	const float cmNear = 6.0f;
	const float cmFar = 18.0f;

	if (cm < cmNear)
		cm = cmNear;
	if (cm > cmFar)
		cm = cmFar;

	float t = (cm - cmNear) / (cmFar - cmNear);

	const int yMin = 1;
	const int yMax = 62 - PADDLE_H;

	int y = (int) ((1.0f - t) * (yMax - yMin) + yMin);

	if (y < yMin) y = yMin;
	if (y > yMax) y = yMax;

	return y;
}
