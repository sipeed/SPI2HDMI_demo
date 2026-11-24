all: build
TARGET:=arcade
TARGET_MCU?=CH32V004
PREFIX?=riscv-none-elf
CH32FUN?=../../ch32fun/ch32fun
ADDITIONAL_C_FILES:=platform.c te.c ui.c fractals.c life.c video.c vendor/mini/termGFX.c vendor/mini/termCharMap.c vendor/mini/rng.c $(wildcard vendor/mini/*run.c vendor/mini/snake.c vendor/mini/pong.c vendor/mini/jumpbird.c vendor/mini/gameover.c) vendor/tiny/invaders.c
EXTRA_CFLAGS+=-Icompat -Ivendor/mini -Ivendor/tiny -include compat/base.h
include $(CH32FUN)/ch32fun.mk
build: $(TARGET).bin
clean: cv_clean
