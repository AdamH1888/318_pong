#ifndef POT_H
#define POT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize both potentiometers on ADC1:
 * - Left paddle  : ADC1_A0 (channel 0, J6[1])
 * - Right paddle : ADC1_A3 (channel 3, J3[2])
 */
void Pot_Init(void);

/* Read left potentiometer raw value (ADC1_A0, 0-4095)
 * Returns -1 on timeout, otherwise returns value in range [0, 4095]
 */
int32_t Pot_ReadRaw(void);

/* Read right potentiometer raw value (ADC1_A3, 0-4095)
 * Returns -1 on timeout, otherwise returns value in range [0, 4095]
 */
int32_t Pot_ReadRightRaw(void);

#ifdef __cplusplus
}
#endif

#endif /* POT_H */
