#ifndef BUZZER_H_
#define BUZZER_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize buzzer on GPIO0_31 */
void Buzzer_Init(void);

/* Update buzzer state (call every frame) */
void Buzzer_Update(void);

/* Trigger buzzer beep for specified number of frames */
void Buzzer_Beep(uint32_t frames);

/* Turn buzzer on immediately */
void Buzzer_On(void);

/* Turn buzzer off immediately */
void Buzzer_Off(void);

/* Stop buzzer and reset counter */
void Buzzer_Stop(void);

#ifdef __cplusplus
}
#endif

#endif /* BUZZER_H_ */
