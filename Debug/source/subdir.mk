################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../source/distance_sensor.c \
../source/draw.c \
../source/framebuffer.c \
../source/game_logic.c \
../source/i2c_bus.c \
../source/lcd.c \
../source/lcd_score.c \
../source/main.c \
../source/oled.c \
../source/semihost_hardfault.c 

C_DEPS += \
./source/distance_sensor.d \
./source/draw.d \
./source/framebuffer.d \
./source/game_logic.d \
./source/i2c_bus.d \
./source/lcd.d \
./source/lcd_score.d \
./source/main.d \
./source/oled.d \
./source/semihost_hardfault.d 

OBJS += \
./source/distance_sensor.o \
./source/draw.o \
./source/framebuffer.o \
./source/game_logic.o \
./source/i2c_bus.o \
./source/lcd.o \
./source/lcd_score.o \
./source/main.o \
./source/oled.o \
./source/semihost_hardfault.o 


# Each subdirectory must supply rules for building sources it contributes
source/%.o: ../source/%.c source/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -std=gnu99 -D__REDLIB__ -DCPU_MCXN947VDF -DCPU_MCXN947VDF_cm33 -DCPU_MCXN947VDF_cm33_core0 -DMCUXPRESSO_SDK -DSDK_DEBUGCONSOLE=1 -DMCUX_META_BUILD -DMCXN947_cm33_core0_SERIES -DCR_INTEGER_PRINTF -DPRINTF_FLOAT_ENABLE=0 -D__MCUXPRESSO -D__USE_CMSIS -DDEBUG -DSDK_OS_BAREMETAL -I"C:\Users\adamh\OneDrive\Documents\PROJECT\frdmmcxn947_hello_world_cm33_core0\drivers" -I"C:\Users\adamh\OneDrive\Documents\PROJECT\frdmmcxn947_hello_world_cm33_core0\CMSIS" -I"C:\Users\adamh\OneDrive\Documents\PROJECT\frdmmcxn947_hello_world_cm33_core0\CMSIS\m-profile" -I"C:\Users\adamh\OneDrive\Documents\PROJECT\frdmmcxn947_hello_world_cm33_core0\device" -I"C:\Users\adamh\OneDrive\Documents\PROJECT\frdmmcxn947_hello_world_cm33_core0\device\periph" -I"C:\Users\adamh\OneDrive\Documents\PROJECT\frdmmcxn947_hello_world_cm33_core0\utilities" -I"C:\Users\adamh\OneDrive\Documents\PROJECT\frdmmcxn947_hello_world_cm33_core0\utilities\str" -I"C:\Users\adamh\OneDrive\Documents\PROJECT\frdmmcxn947_hello_world_cm33_core0\utilities\debug_console_lite" -I"C:\Users\adamh\OneDrive\Documents\PROJECT\frdmmcxn947_hello_world_cm33_core0\component\uart" -I"C:\Users\adamh\OneDrive\Documents\PROJECT\frdmmcxn947_hello_world_cm33_core0\source" -I"C:\Users\adamh\OneDrive\Documents\PROJECT\frdmmcxn947_hello_world_cm33_core0\board" -O0 -fno-common -g3 -gdwarf-4 -mcpu=cortex-m33 -c -ffunction-sections -fdata-sections -fno-builtin -imacros "C:\Users\adamh\OneDrive\Documents\PROJECT\frdmmcxn947_hello_world_cm33_core0\source\mcux_config.h" -fmerge-constants -fmacro-prefix-map="$(<D)/"= -mcpu=cortex-m33 -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -D__REDLIB__ -fstack-usage -specs=redlib.specs -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-source

clean-source:
	-$(RM) ./source/distance_sensor.d ./source/distance_sensor.o ./source/draw.d ./source/draw.o ./source/framebuffer.d ./source/framebuffer.o ./source/game_logic.d ./source/game_logic.o ./source/i2c_bus.d ./source/i2c_bus.o ./source/lcd.d ./source/lcd.o ./source/lcd_score.d ./source/lcd_score.o ./source/main.d ./source/main.o ./source/oled.d ./source/oled.o ./source/semihost_hardfault.d ./source/semihost_hardfault.o

.PHONY: clean-source

