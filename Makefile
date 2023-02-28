# Hisilicon Hi35xx sample Makefile

include ./Makefile.param

SRCS := $(wildcard *.c    ./Common/*.c   ./Utils/CircleBuf/*.c     ./Utils/ImageProcessing/*.c    ./Utils/Led/*.c   ./Utils/Log/*.c   ./Utils/Rtsp/*.c  )
TARGET := $(SRCS:%.c=%)

TARGET_PATH := $(PWD)

# compile linux or HuaweiLite
include $(PWD)/$(ARM_ARCH)_$(OSTYPE).mak

