#include "pot.h"
#include "fsl_lpadc.h"
#include "fsl_common.h"
#include "fsl_clock.h"

/* ADC1 on MCXN947 */
#define POT_ADC_BASE                ADC1

/* Left potentiometer: ADC1_A0 -> channel 0 (J6[1]) */
#define POT_LEFT_ADC_CHANNEL        0U
#define POT_LEFT_ADC_TRIGGER_ID     0U
#define POT_LEFT_ADC_CMD_ID         1U

/* Right potentiometer: ADC1_A3 -> channel 3 (J3[2]) */
#define POT_RIGHT_ADC_CHANNEL       3U
#define POT_RIGHT_ADC_TRIGGER_ID    1U
#define POT_RIGHT_ADC_CMD_ID        2U

#define POT_POLL_TIMEOUT            (50000UL)

/* ADC result constants */
#define POT_ADC_MAX_12BIT           (4095U)   /* 12-bit max value */

static lpadc_conv_result_t g_potAdcResult;

static int32_t Pot_ReadByTrigger(uint32_t triggerId)
{
    uint32_t timeout = POT_POLL_TIMEOUT;
    bool gotResult = false;

    /* Trigger one conversion */
    LPADC_DoSoftwareTrigger(POT_ADC_BASE, (1UL << triggerId));

    /* Poll for result with timeout */
    while (timeout--)
    {
        if (LPADC_GetConvResult(POT_ADC_BASE, &g_potAdcResult, 0U))
        {
            gotResult = true;
            break;
        }
    }

    if (!gotResult)
    {
        return -1;  /* Timeout */
    }

    /* MCXN LPADC 12-bit result: right shift by 3 to get 0-4095 */
    uint32_t conv = (uint32_t)g_potAdcResult.convValue;
    uint32_t raw = (conv >> 3U);

    if (raw > POT_ADC_MAX_12BIT)
    {
        raw = POT_ADC_MAX_12BIT;
    }

    return (int32_t)raw;
}

/* Initialize both potentiometer ADC commands/triggers */
void Pot_Init(void)
{
    lpadc_config_t lpadcConfig;
    lpadc_conv_command_config_t cmdConfig;
    lpadc_conv_trigger_config_t trigConfig;

    /* Enable ADC1 clock */
    CLOCK_EnableClock(kCLOCK_Adc1);

    /* Initialize ADC1 (BOARD_InitHardware does not call BOARD_InitBootPeripherals,
       so peripherals.c never runs — this is the only ADC1 init) */
    LPADC_GetDefaultConfig(&lpadcConfig);
    LPADC_Init(POT_ADC_BASE, &lpadcConfig);

    /* Configure conversion command for left potentiometer (ADC1_A0 / channel 0) */
    LPADC_GetDefaultConvCommandConfig(&cmdConfig);
    cmdConfig.channelNumber = POT_LEFT_ADC_CHANNEL;
    LPADC_SetConvCommandConfig(POT_ADC_BASE, POT_LEFT_ADC_CMD_ID, &cmdConfig);

    /* Configure conversion command for right potentiometer (ADC1_A3 / channel 3) */
    LPADC_GetDefaultConvCommandConfig(&cmdConfig);
    cmdConfig.channelNumber = POT_RIGHT_ADC_CHANNEL;
    LPADC_SetConvCommandConfig(POT_ADC_BASE, POT_RIGHT_ADC_CMD_ID, &cmdConfig);

    /* Configure software trigger for left potentiometer */
    LPADC_GetDefaultConvTriggerConfig(&trigConfig);
    trigConfig.targetCommandId = POT_LEFT_ADC_CMD_ID;
    trigConfig.enableHardwareTrigger = false;
    LPADC_SetConvTriggerConfig(POT_ADC_BASE, POT_LEFT_ADC_TRIGGER_ID, &trigConfig);

    /* Configure software trigger for right potentiometer */
    LPADC_GetDefaultConvTriggerConfig(&trigConfig);
    trigConfig.targetCommandId = POT_RIGHT_ADC_CMD_ID;
    trigConfig.enableHardwareTrigger = false;
    LPADC_SetConvTriggerConfig(POT_ADC_BASE, POT_RIGHT_ADC_TRIGGER_ID, &trigConfig);
}

/* Read left potentiometer raw value (0-4095) */
int32_t Pot_ReadRaw(void)
{
    return Pot_ReadByTrigger(POT_LEFT_ADC_TRIGGER_ID);
}

/* Read right potentiometer raw value (0-4095) */
int32_t Pot_ReadRightRaw(void)
{
    return Pot_ReadByTrigger(POT_RIGHT_ADC_TRIGGER_ID);
}
