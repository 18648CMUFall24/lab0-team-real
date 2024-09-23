#!/bin/bash -p

adb pull /proc/config.gz
gunzip config.gz
mv config .config
export CROSS_COMPILE=arm-linux-gnuabi- ARCH=arm
make oldconfig
make
file arch/arm/boot/compressed/vmlinux

