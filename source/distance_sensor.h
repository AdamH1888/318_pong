#ifndef HCSR04_H
#define HCSR04_H

#include <stdbool.h>
#include <stdint.h>
#include "fsl_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    GPIO_Type *trigGpio;
    uint32_t trigPin;

    GPIO_Type *echoGpio;
    uint32_t echoPin;
} hcsr04_t;

void HCSR04_Init(const hcsr04_t *dev);

/* Returns true if valid measurement; distance_cm written to *outCm */
bool HCSR04_ReadCm(const hcsr04_t *dev, float *outCm);

/* Convert measured hand distance (cm) into a paddle top-edge Y position */
int HCSR04_MapToPaddleY(float cm);

#ifdef __cplusplus
}
#endif

#endif /* HCSR04_H */
