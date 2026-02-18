#include "distance_sensor.h"
#include "fsl_gpio.h"
#include "fsl_device_registers.h"
#include "clock_config.h"
#include "fsl_common.h"

//Ensures the SysTick timer is running
static void ensure_systick_running(void)
{
    //Check if SysTick is already running, checks if ENABLE bit is set
    if ((SysTick->CTRL & SysTick_CTRL_ENABLE_Msk) == 0u)
    {
        //Set the reload value to maximum, this is the timer's maximum count
        SysTick->LOAD = 0x00FFFFFFu;
        //Clear current value of the SysTick timer (reset the timer)
        SysTick->VAL = 0u;
        //Enable SysTick with the system clock as its source
        SysTick->CTRL = SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_CLKSOURCE_Msk;
    }
}

//Initialise the HC-SR04 distance sensor
void HCSR04_Init(const hcsr04_t *dev)
{
    //Ensure that SysTick is running
    ensure_systick_running();

    //Setup the TRIG pin as a digital output, used to trigger the ultrasonic pulse
    gpio_pin_config_t trig_config = {kGPIO_DigitalOutput, 0};
    //Setup the ECHO pin as a digital input, used to measure the time it takes for the echo to return
    gpio_pin_config_t echo_config = {kGPIO_DigitalInput, 0};

    //Initialise the TRIG pin and ECHO pin on their respective GPIO ports
    GPIO_PinInit(dev->trigGpio, dev->trigPin, &trig_config);
    GPIO_PinInit(dev->echoGpio, dev->echoPin, &echo_config);

    //Set the TRIG pin to LOW, make sure no signal is being sent initially
    GPIO_PinWrite(dev->trigGpio, dev->trigPin, 0u);
}

//Read the distance measured by the HC-SR04 sensor in cm
bool HCSR04_ReadCm(const hcsr04_t *dev, float *outCm)
{
    uint32_t start_tick, end_tick, elapsed_ticks;
    uint32_t timeout_cnt = 0;

    //Send a 10-microsecond pulse on the TRIG pin to initiate the measurement
    GPIO_PinWrite(dev->trigGpio, dev->trigPin, 1u);
    SDK_DelayAtLeastUs(10u, SystemCoreClock);  //Wait for at least 10 microseconds
    GPIO_PinWrite(dev->trigGpio, dev->trigPin, 0u);  //Stop sending the pulse

    //Wait for the ECHO pin to go high, this shows pulse has been received
    while (GPIO_PinRead(dev->echoGpio, dev->echoPin) == 0u)
    {
        if (++timeout_cnt > 1000000u) return false;  //Timeout if no signal is received
    }
    start_tick = SysTick->VAL;  //Record the time when the pulse is received

    timeout_cnt = 0;
    //Wait for the ECHO pin to go low, indicates the end of the echo
    while (GPIO_PinRead(dev->echoGpio, dev->echoPin) == 1u)
    {
        if (++timeout_cnt > 1000000u) return false;  //Timeout if no signal is received
    }
    end_tick = SysTick->VAL;  //Record the time when the pulse ends

    //Calculate the elapsed ticks (time it took for the signal to travel to the object and back)
    if (start_tick > end_tick)
        elapsed_ticks = start_tick - end_tick;  //If the timer rolled over
    else
        elapsed_ticks = (0x00FFFFFFu - end_tick) + start_tick;  //If no rollover

    //Convert the time into a distance using speed of sound
    float duration_us = (float)elapsed_ticks / (SystemCoreClock / 1000000.0f);  //Convert ticks to microseconds
    float dist_cm = (duration_us * 0.034f) / 2.0f;  //Calculate distance in cm

    //Ensure the distance is within a reasonable range
    if (dist_cm < 2.0f || dist_cm > 400.0f) return false;

    //Output the distance
    *outCm = dist_cm;
    return true;
}
