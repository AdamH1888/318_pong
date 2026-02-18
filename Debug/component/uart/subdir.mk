################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../component/uart/fsl_adapter_lpuart.c 

C_DEPS += \
./component/uart/fsl_adapter_lpuart.d 

OBJS += \
./component/uart/fsl_adapter_lpuart.o 


# Each subdirectory must supply rules for building sources it contributes
component/uart/%.o: ../component/uart/%.c component/uart/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -std=gnu99 -D__REDLIB__ -DCPU_MCXN947VDF -DCPU_MCXN947VDF_cm33 -DCPU_MCXN947VDF_cm33_core0 -DMCUXPRESSO_SDK -DSDK_DEBUGCONSOLE=1 -DMCUX_META_BUILD -DMCXN947_cm33_core0_SERIES -DCR_INTEGER_PRINTF -DPRINTF_FLOAT_ENABLE=0 -D__MCUXPRESSO -D__USE_CMSIS -DDEBUG -DSDK_OS_BAREMETAL -I"C:\Users\adamh\OneDrive\Documents\PROJECT\frdmmcxn947_hello_world_cm33_core0\drivers" -I"C:\Users\adamh\OneDrive\Documents\PROJECT\frdmmcxn947_hello_world_cm33_core0\CMSIS" -I"C:\Users\adamh\OneDrive\Documents\PROJECT\frdmmcxn947_hello_world_cm33_core0\CMSIS\m-profile" -I"C:\Users\adamh\OneDrive\Documents\PROJECT\frdmmcxn947_hello_world_cm33_core0\device" -I"C:\Users\adamh\OneDrive\Documents\PROJECT\frdmmcxn947_hello_world_cm33_core0\device\periph" -I"C:\Users\adamh\OneDrive\Documents\PROJECT\frdmmcxn947_hello_world_cm33_core0\utilities" -I"C:\Users\adamh\OneDrive\Documents\PROJECT\frdmmcxn947_hello_world_cm33_core0\utilities\str" -I"C:\Users\adamh\OneDrive\Documents\PROJECT\frdmmcxn947_hello_world_cm33_core0\utilities\debug_console_lite" -I"C:\Users\adamh\OneDrive\Documents\PROJECT\frdmmcxn947_hello_world_cm33_core0\component\uart" -I"C:\Users\adamh\OneDrive\Documents\PROJECT\frdmmcxn947_hello_world_cm33_core0\source" -I"C:\Users\adamh\OneDrive\Documents\PROJECT\frdmmcxn947_hello_world_cm33_core0\board" -O0 -fno-common -g3 -gdwarf-4 -mcpu=cortex-m33 -c -ffunction-sections -fdata-sections -fno-builtin -imacros "C:\Users\adamh\OneDrive\Documents\PROJECT\frdmmcxn947_hello_world_cm33_core0\source\mcux_config.h" -fmerge-constants -fmacro-prefix-map="$(<D)/"= -mcpu=cortex-m33 -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -D__REDLIB__ -fstack-usage -specs=redlib.specs -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-component-2f-uart

clean-component-2f-uart:
	-$(RM) ./component/uart/fsl_adapter_lpuart.d ./component/uart/fsl_adapter_lpuart.o

.PHONY: clean-component-2f-uart

