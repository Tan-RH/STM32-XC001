# XC001 build entry points, aligned with the Chaos build workflow.
#
# make bootloader  -> build permanent bootloader HEX
# make app / dfu   -> build application HEX and BIN
# make factory     -> build both and generate a one-shot factory HEX

PYTHON ?= python
APP_BUILD_DIR ?= Debug
BOOTLOADER_DIR ?= Bootloader
APP_HEX ?= ../$(APP_BUILD_DIR)/XC001.hex

.PHONY: all bootloader app dfu factory clean version sanitize-cube-makefiles

all: bootloader app

bootloader:
	$(MAKE) -C $(BOOTLOADER_DIR) all

sanitize-cube-makefiles:
	$(PYTHON) Tools/sanitize_cube_makefiles.py $(APP_BUILD_DIR)

app dfu: sanitize-cube-makefiles
	$(MAKE) -C $(APP_BUILD_DIR) all XC001_BUILD_FACTORY=0

factory: app bootloader
	$(MAKE) -C $(BOOTLOADER_DIR) factory APP_HEX=$(APP_HEX)

version:
	$(PYTHON) Tools/package_firmware.py --print-version

clean:
	$(MAKE) -C $(BOOTLOADER_DIR) clean
	$(MAKE) -C $(APP_BUILD_DIR) clean
