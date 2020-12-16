################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/OLED.c \
../src/background.c \
../src/ball.c \
../src/bounce.c \
../src/fifo.c \
../src/lcd.c \
../src/music.c \
../src/paddle.c \
../src/syscalls.c \
../src/system_stm32f0xx.c \
../src/tty.c 

OBJS += \
./src/OLED.o \
./src/background.o \
./src/ball.o \
./src/bounce.o \
./src/fifo.o \
./src/lcd.o \
./src/music.o \
./src/paddle.o \
./src/syscalls.o \
./src/system_stm32f0xx.o \
./src/tty.o 

C_DEPS += \
./src/OLED.d \
./src/background.d \
./src/ball.d \
./src/bounce.d \
./src/fifo.d \
./src/lcd.d \
./src/music.d \
./src/paddle.d \
./src/syscalls.d \
./src/system_stm32f0xx.d \
./src/tty.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MCU GCC Compiler'
	@echo $(PWD)
	arm-none-eabi-gcc -mcpu=cortex-m0 -mthumb -mfloat-abi=soft -DSTM32 -DSTM32F0 -DSTM32F091RCTx -DDEBUG -DSTM32F091 -DUSE_STDPERIPH_DRIVER -I"/Users/matteomiglio/Documents/lab11/StdPeriph_Driver/inc" -I"/Users/matteomiglio/Documents/lab11/inc" -I"/Users/matteomiglio/Documents/lab11/CMSIS/device" -I"/Users/matteomiglio/Documents/lab11/CMSIS/core" -O0 -g3 -Wall -fmessage-length=0 -ffunction-sections -c -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


