#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#include "fsl_clock.h"

void BOARD_InitHardware(void) {
	/* attach FRO 12M to FLEXCOMM4 (debug console) */
	CLOCK_SetClkDiv(kCLOCK_DivFlexcom4Clk, 1u);
	CLOCK_AttachClk(BOARD_DEBUG_UART_CLK_ATTACH);

	/* attach TRACECLKDIV to TRACE */
	CLOCK_SetClkDiv(kCLOCK_DivTraceClk, 2U);
	CLOCK_AttachClk(kTRACE_DIV_to_TRACE);

	BOARD_InitBootPins();
	BOARD_InitBootClocks();
	BOARD_InitDebugConsole();

	// Configure the clock for I2C
	CLOCK_SetClkDiv(kCLOCK_DivFlexcom2Clk, 1u);
	CLOCK_AttachClk(kFRO12M_to_FLEXCOMM2);
	// Add a small delay prior to device initialisation
	SDK_DelayAtLeastUs(1000000, CLOCK_GetFreq(kCLOCK_CoreSysClk));
}

