#SYSROOT=$(HOME)/android-ndk-r9/platforms/android-14/arch-arm
#LIBSTDCPP=$(HOME)/android-ndk-r9/sources/cxx-stl/gnu-libstdc++

EXTRA_CFLAGS=-fsigned-char -march=armv7-a -mfloat-abi=softfp \
-mfpu=vfp -fdata-sections -ffunction-sections -fexceptions -mthumb \
-fPIC -Wno-psabi -DANDROID -D__ARM_ARCH_5__ -D__ARM_ARCH_5T__ \
-D__ARM_ARCH_5E__ -D__ARM_ARCH_5TE__ -fomit-frame-pointer \
--sysroot $(SYSROOT) $(INCLUDES)

EXTRA_LDFLAGS=-rdynamic --sysroot $(SYSROOT) -Wl,--gc-sections \
-L$(LIBSTDCPP)/$(NDK_GCC)/libs/armeabi-v7a -lgnustl_static -lsupc++

EXP_HDRS=$(HOME)/Documents/lab0-team-real/usr/include
CC=arm-linux-androideabi-gcc

hello: hello.c
	$(CC) -o hello -isystem $(EXP_HDRS) $(EXTRA_CFLAGS) $(EXTRA_LDFLAGS) hello.c

