PORT ?= /dev/ttyACM0

IDF_PATH ?= $(shell cat .IDF_PATH 2>/dev/null || echo `pwd`/esp-idf)
IDF_TOOLS_PATH ?= $(shell cat .IDF_TOOLS_PATH 2>/dev/null || echo `pwd`/esp-idf-tools)
IDF_REPO ?= https://github.com/espressif/esp-idf.git
# ESP-IDF v6.0.2, which is what badge-bsp 1.1 and up ask for.
IDF_COMMIT ?= 7101770dc6db2667b3c477cc31365dd1acd6db4e
IDF_EXPORT_QUIET ?= 1
IDF_GITHUB_ASSETS ?= dl.espressif.com/github_assets
MAKEFLAGS += --silent

SHELL := /usr/bin/env bash

DEVICE ?= tanmatsu # Default target device
BUILD ?= build/$(DEVICE)

export IDF_TOOLS_PATH
export IDF_GITHUB_ASSETS

# General targets

.PHONY: all
all: build

# Preparation

.PHONY: prepare
prepare: submodules sdk

.PHONY: submodules
submodules: 
	if [ ! -f .submodules_update_done ]; then \
		echo "Updating submodules"; \
		git submodule update --init --recursive; \
		touch .submodules_update_done; \
	fi

.PHONY: sdk
sdk:
	if test -d "$(IDF_PATH)"; then echo -e "ESP-IDF target folder exists!\r\nPlease remove the folder or un-set the environment variable."; exit 1; fi
	if test -d "$(IDF_TOOLS_PATH)"; then echo -e "ESP-IDF tools target folder exists!\r\nPlease remove the folder or un-set the environment variable."; exit 1; fi
	# Fetch the pinned commit directly. Cloning a branch and checking out the commit afterwards
	# leaves the submodules on the branch tip, which conflicts once the branch has moved on.
	git init -q "$(IDF_PATH)"
	cd "$(IDF_PATH)"; git remote add origin "$(IDF_REPO)"
	cd "$(IDF_PATH)"; git fetch --depth=1 origin "$(IDF_COMMIT)"
	cd "$(IDF_PATH)"; git checkout --detach FETCH_HEAD
	# Not every submodule host serves unadvertised objects, so fall back to a full clone of those.
	cd "$(IDF_PATH)"; git submodule update --init --recursive --depth=1 || git submodule update --init --recursive
	cd "$(IDF_PATH)"; bash install.sh all

.PHONY: removesdk
removesdk:
	rm -rf "$(IDF_PATH)"
	rm -rf "$(IDF_TOOLS_PATH)"

.PHONY: refreshsdk
refreshsdk: removesdk sdk

.PHONY: menuconfig
menuconfig:
	source "$(IDF_PATH)/export.sh" && idf.py menuconfig -DDEVICE=$(DEVICE)
	
# Cleaning

.PHONY: clean
clean:
	rm -rf $(BUILD)
	rm -f .submodules_update_done

.PHONY: fullclean
fullclean: clean
	rm -f sdkconfig
	rm -f sdkconfig.old
	rm -f sdkconfig.ci
	rm -f sdkconfig.defaults

# Check if build environment is set up correctly
.PHONY: checkbuildenv
checkbuildenv:
	if [ -z "$(IDF_PATH)" ]; then echo "IDF_PATH is not set!"; exit 1; fi
	if [ -z "$(IDF_TOOLS_PATH)" ]; then echo "IDF_TOOLS_PATH is not set!"; exit 1; fi
	# Check if the IDF commit id the one we need
	#if [ -d "$(IDF_PATH)" ]; then \
	#	if [ "$(IDF_COMMIT)" != "$(shell cd $(IDF_PATH); git rev-parse HEAD)" ]; then \
	#		echo "ESP-IDF commit id does not match! Expected '$(IDF_COMMIT)' got '$(shell git rev-parse HEAD)'"; \
	#		echo "Run $ make refreshsdk"; \
	#		echo "To update the ESP-IDF to the correct commit id"; \
	#		echo "Or set the IDF_COMMIT variable in the Makefile to the correct commit id"; \
	#		exit 1; \
	#	fi; \
	#fi

# Building

.PHONY: build
build: checkbuildenv submodules
	source "$(IDF_PATH)/export.sh" >/dev/null && idf.py -B $(BUILD) build -DDEVICE=$(DEVICE)

# Hardware

.PHONY: flash
flash: build
	source "$(IDF_PATH)/export.sh" && \
	idf.py -B $(BUILD) flash -p $(PORT)

.PHONY: flashmonitor
flashmonitor: build
	source "$(IDF_PATH)/export.sh" && \
	idf.py -B $(BUILD) flash -p $(PORT) monitor

.PHONY: erase
erase:
	source "$(IDF_PATH)/export.sh" && idf.py -B $(BUILD) erase-flash -p $(PORT)

.PHONY: monitor
monitor:
	source "$(IDF_PATH)/export.sh" && idf.py -B $(BUILD) monitor -p $(PORT)

.PHONY: openocd
openocd:
	source "$(IDF_PATH)/export.sh" && idf.py -B $(BUILD) openocd

.PHONY: gdb
gdb:
	source "$(IDF_PATH)/export.sh" && idf.py -B $(BUILD) gdb

# Tools

.PHONY: size
size:
	source "$(IDF_PATH)/export.sh" && idf.py -B $(BUILD) size

.PHONY: size-components
size-components:
	source "$(IDF_PATH)/export.sh" && idf.py -B $(BUILD) size-components

.PHONY: size-files
size-files:
	source "$(IDF_PATH)/export.sh" && idf.py -B $(BUILD) size-files

# Formatting

.PHONY: format
format:
	find main/ -iname '*.h' -o -iname '*.c' -o -iname '*.cpp' | xargs clang-format -i

# Badgelink
.PHONY: badgelink
badgelink:
	rm -rf badgelink
	git clone https://github.com/badgeteam/esp32-component-badgelink.git badgelink
	cd badgelink/tools; ./install.sh

.PHONY: install
install:
	cd badgelink/tools; ./badgelink.sh appfs upload application "USB HID host" 0 ../../build/tanmatsu/application.bin

.PHONY: run
run:
	cd badgelink/tools; ./badgelink.sh start application
