################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/fdcan.c \
../Core/Src/freertos.c \
../Core/Src/gpio.c \
../Core/Src/main.c \
../Core/Src/spi.c \
../Core/Src/stm32h7xx_hal_msp.c \
../Core/Src/stm32h7xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32h7xx.c \
../Core/Src/usart.c \
../Core/Src/xc001_app.c \
../Core/Src/xc001_board.c \
../Core/Src/xc001_can.c \
../Core/Src/xc001_config.c \
../Core/Src/xc001_console.c \
../Core/Src/xc001_multipart.c \
../Core/Src/xc001_net.c \
../Core/Src/xc001_rs485.c \
../Core/Src/xc001_scpi.c \
../Core/Src/xc001_spi_bus.c \
../Core/Src/xc001_storage.c \
../Core/Src/xc001_utils.c 

OBJS += \
./Core/Src/fdcan.o \
./Core/Src/freertos.o \
./Core/Src/gpio.o \
./Core/Src/main.o \
./Core/Src/spi.o \
./Core/Src/stm32h7xx_hal_msp.o \
./Core/Src/stm32h7xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32h7xx.o \
./Core/Src/usart.o \
./Core/Src/xc001_app.o \
./Core/Src/xc001_board.o \
./Core/Src/xc001_can.o \
./Core/Src/xc001_config.o \
./Core/Src/xc001_console.o \
./Core/Src/xc001_multipart.o \
./Core/Src/xc001_net.o \
./Core/Src/xc001_rs485.o \
./Core/Src/xc001_scpi.o \
./Core/Src/xc001_spi_bus.o \
./Core/Src/xc001_storage.o \
./Core/Src/xc001_utils.o 

C_DEPS += \
./Core/Src/fdcan.d \
./Core/Src/freertos.d \
./Core/Src/gpio.d \
./Core/Src/main.d \
./Core/Src/spi.d \
./Core/Src/stm32h7xx_hal_msp.d \
./Core/Src/stm32h7xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32h7xx.d \
./Core/Src/usart.d \
./Core/Src/xc001_app.d \
./Core/Src/xc001_board.d \
./Core/Src/xc001_can.d \
./Core/Src/xc001_config.d \
./Core/Src/xc001_console.d \
./Core/Src/xc001_multipart.d \
./Core/Src/xc001_net.d \
./Core/Src/xc001_rs485.d \
./Core/Src/xc001_scpi.d \
./Core/Src/xc001_spi_bus.d \
./Core/Src/xc001_storage.d \
./Core/Src/xc001_utils.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_PWR_LDO_SUPPLY -DUSE_HAL_DRIVER -DSTM32H743xx -c -I../Core/Inc -I../LWIP/App -I../LWIP/Target -I../Middlewares/Third_Party/LwIP/src/include -I../Middlewares/Third_Party/LwIP/system -I../Drivers/STM32H7xx_HAL_Driver/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../Drivers/CMSIS/RTOS2/Include -I../Drivers/BSP/Components/lan8742 -I../Middlewares/Third_Party/LwIP/src/include/netif/ppp -I../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../Middlewares/Third_Party/LwIP/src/include/lwip -I../Middlewares/Third_Party/LwIP/src/include/lwip/apps -I../Middlewares/Third_Party/LwIP/src/include/lwip/priv -I../Middlewares/Third_Party/LwIP/src/include/lwip/prot -I../Middlewares/Third_Party/LwIP/src/include/netif -I../Middlewares/Third_Party/LwIP/src/include/compat/posix -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/arpa -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/net -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/sys -I../Middlewares/Third_Party/LwIP/src/include/compat/stdc -I../Middlewares/Third_Party/LwIP/system/arch -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/fdcan.cyclo ./Core/Src/fdcan.d ./Core/Src/fdcan.o ./Core/Src/fdcan.su ./Core/Src/freertos.cyclo ./Core/Src/freertos.d ./Core/Src/freertos.o ./Core/Src/freertos.su ./Core/Src/gpio.cyclo ./Core/Src/gpio.d ./Core/Src/gpio.o ./Core/Src/gpio.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/spi.cyclo ./Core/Src/spi.d ./Core/Src/spi.o ./Core/Src/spi.su ./Core/Src/stm32h7xx_hal_msp.cyclo ./Core/Src/stm32h7xx_hal_msp.d ./Core/Src/stm32h7xx_hal_msp.o ./Core/Src/stm32h7xx_hal_msp.su ./Core/Src/stm32h7xx_it.cyclo ./Core/Src/stm32h7xx_it.d ./Core/Src/stm32h7xx_it.o ./Core/Src/stm32h7xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32h7xx.cyclo ./Core/Src/system_stm32h7xx.d ./Core/Src/system_stm32h7xx.o ./Core/Src/system_stm32h7xx.su ./Core/Src/usart.cyclo ./Core/Src/usart.d ./Core/Src/usart.o ./Core/Src/usart.su ./Core/Src/xc001_app.cyclo ./Core/Src/xc001_app.d ./Core/Src/xc001_app.o ./Core/Src/xc001_app.su ./Core/Src/xc001_board.cyclo ./Core/Src/xc001_board.d ./Core/Src/xc001_board.o ./Core/Src/xc001_board.su ./Core/Src/xc001_can.cyclo ./Core/Src/xc001_can.d ./Core/Src/xc001_can.o ./Core/Src/xc001_can.su ./Core/Src/xc001_config.cyclo ./Core/Src/xc001_config.d ./Core/Src/xc001_config.o ./Core/Src/xc001_config.su ./Core/Src/xc001_console.cyclo ./Core/Src/xc001_console.d ./Core/Src/xc001_console.o ./Core/Src/xc001_console.su ./Core/Src/xc001_multipart.cyclo ./Core/Src/xc001_multipart.d ./Core/Src/xc001_multipart.o ./Core/Src/xc001_multipart.su ./Core/Src/xc001_net.cyclo ./Core/Src/xc001_net.d ./Core/Src/xc001_net.o ./Core/Src/xc001_net.su ./Core/Src/xc001_rs485.cyclo ./Core/Src/xc001_rs485.d ./Core/Src/xc001_rs485.o ./Core/Src/xc001_rs485.su ./Core/Src/xc001_scpi.cyclo ./Core/Src/xc001_scpi.d ./Core/Src/xc001_scpi.o ./Core/Src/xc001_scpi.su ./Core/Src/xc001_spi_bus.cyclo ./Core/Src/xc001_spi_bus.d ./Core/Src/xc001_spi_bus.o ./Core/Src/xc001_spi_bus.su ./Core/Src/xc001_storage.cyclo ./Core/Src/xc001_storage.d ./Core/Src/xc001_storage.o ./Core/Src/xc001_storage.su ./Core/Src/xc001_utils.cyclo ./Core/Src/xc001_utils.d ./Core/Src/xc001_utils.o ./Core/Src/xc001_utils.su

.PHONY: clean-Core-2f-Src

