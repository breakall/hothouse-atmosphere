TARGET = atmosphere
APP_TYPE = BOOT_SRAM
OPT = -O3
CPP_STANDARD = -std=gnu++17
CPP_SOURCES = main.cpp engine.cpp debug.cpp vendor/hothouse.cpp
C_INCLUDES = -I. -Ivendor
LIBDAISY_DIR = ../deps/libDaisy
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
