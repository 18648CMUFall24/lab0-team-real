#!/bin/bash -p

adb pull /proc/config.gz
adb reboot-bootloader
gunzip config.gz
mv config .config
export CROSS_COMPILE=arm-linux-gnuabi- ARCH=arm
make oldconfig
make
RAMDISK=~/nakasi-jdq39/boot-img/boot.img-ramdisk-root.gz
fastboot boot arch/arm/boot/zImage $RAMDISK

