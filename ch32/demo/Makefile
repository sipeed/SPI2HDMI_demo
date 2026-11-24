all : build

TARGET:=demo
# TARGET_MCU=CH32V004 make   -> 160x120 grid (6KB RAM parts)
TARGET_MCU?=CH32V003

PREFIX?=riscv-none-elf

include ../../ch32fun/ch32fun/ch32fun.mk

build : $(TARGET).bin
flash : cv_flash
clean : cv_clean
