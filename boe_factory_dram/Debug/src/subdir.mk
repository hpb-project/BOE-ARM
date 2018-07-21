################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
LD_SRCS += \
../src/lscript.ld 

C_SRCS += \
../src/common_functions.c \
../src/memtest.c \
../src/platform.c \
../src/rd_eye.c \
../src/wr_eye.c 

OBJS += \
./src/common_functions.o \
./src/memtest.o \
./src/platform.o \
./src/rd_eye.o \
./src/wr_eye.o 

C_DEPS += \
./src/common_functions.d \
./src/memtest.d \
./src/platform.d \
./src/rd_eye.d \
./src/wr_eye.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: ARM v8 gcc compiler'
	aarch64-none-elf-gcc -Wall -O0 -g3 -c -fmessage-length=0 -MT"$@" -I../../boe_factory_fsbl_bsp/psu_cortexa53_0/include -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


