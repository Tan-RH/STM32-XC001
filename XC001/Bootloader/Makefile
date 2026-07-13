PROJECT_ROOT := ..
BUILD_DIR := build
TOOL_PREFIX ?= arm-none-eabi-
CC := $(TOOL_PREFIX)gcc
OBJCOPY := $(TOOL_PREFIX)objcopy
SIZE := $(TOOL_PREFIX)size
PYTHON ?= python
APP_HEX ?= $(PROJECT_ROOT)/Debug/XC001.hex

CPU_FLAGS := -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb
CFLAGS := $(CPU_FLAGS) -std=gnu11 -Os -Wall -Wextra -ffunction-sections -fdata-sections \
  -DSTM32H743xx -DUSE_HAL_DRIVER -I$(PROJECT_ROOT)/Core/Inc \
  -I$(PROJECT_ROOT)/Drivers/CMSIS/Include \
  -I$(PROJECT_ROOT)/Drivers/CMSIS/Device/ST/STM32H7xx/Include \
  -I$(PROJECT_ROOT)/Drivers/STM32H7xx_HAL_Driver/Inc \
  -I$(PROJECT_ROOT)/Drivers/STM32H7xx_HAL_Driver/Inc/Legacy
LDFLAGS := $(CPU_FLAGS) --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections -static -Wl,--start-group -lc -lm -Wl,--end-group

COMMON_SRC := $(PROJECT_ROOT)/Core/Startup/startup_stm32h743zitx.s \
  $(PROJECT_ROOT)/Core/Src/system_stm32h7xx.c \
  $(PROJECT_ROOT)/Core/Src/syscalls.c $(PROJECT_ROOT)/Core/Src/sysmem.c
HAL_SRC := $(PROJECT_ROOT)/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_flash.c \
  $(PROJECT_ROOT)/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_flash_ex.c

.PHONY: all clean stage0 recovery bootloader-image bootloader-package factory
all: stage0 recovery bootloader-image bootloader-package

stage0: $(BUILD_DIR)/XC001_Stage0.bin $(BUILD_DIR)/XC001_Stage0.hex
recovery: $(BUILD_DIR)/XC001_Recovery.bin $(BUILD_DIR)/XC001_Recovery.hex
bootloader-image: $(BUILD_DIR)/XC001_Bootloader.hex
bootloader-package: bootloader-image
	$(PYTHON) $(PROJECT_ROOT)/Tools/package_firmware.py --bootloader $(BUILD_DIR)/XC001_Bootloader.hex

factory: all
	$(PYTHON) make_factory_image.py --stage0 $(BUILD_DIR)/XC001_Stage0.hex --app $(APP_HEX) --recovery $(BUILD_DIR)/XC001_Recovery.hex --output $(BUILD_DIR)/XC001_Factory.hex
	$(PYTHON) $(PROJECT_ROOT)/Tools/package_firmware.py --factory $(BUILD_DIR)/XC001_Factory.hex

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/XC001_Stage0.elf: stage0_main.c stage0.ld | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(COMMON_SRC) stage0_main.c -Tstage0.ld $(LDFLAGS) -Wl,-Map=$(BUILD_DIR)/XC001_Stage0.map -o $@
	$(SIZE) $@

$(BUILD_DIR)/XC001_Bootloader.hex: $(BUILD_DIR)/XC001_Stage0.hex $(BUILD_DIR)/XC001_Recovery.hex make_bootloader_image.py | $(BUILD_DIR)
	$(PYTHON) make_bootloader_image.py --stage0 $(BUILD_DIR)/XC001_Stage0.hex --recovery $(BUILD_DIR)/XC001_Recovery.hex --output $@
$(BUILD_DIR)/XC001_Recovery.elf: recovery_main.c recovery.ld | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(COMMON_SRC) $(HAL_SRC) recovery_main.c -Trecovery.ld $(LDFLAGS) -Wl,-Map=$(BUILD_DIR)/XC001_Recovery.map -o $@
	$(SIZE) $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf
	$(OBJCOPY) -O ihex $< $@

clean:
	rm -rf $(BUILD_DIR)
