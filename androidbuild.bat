@echo off

rem SETTINGS
set SDK_ROOT=c:\Android\sdk
set NDK_ROOT=c:\Android\android-ndk-r10e
set SYSROOT=%NDK_ROOT%\platforms\android-21\arch-arm
set CC=%NDK_ROOT%\toolchains\arm-linux-androideabi-4.9\prebuilt\windows-x86_64\bin\arm-linux-androideabi-gcc --sysroot=%SYSROOT%

rem BUILD
rd /S /Q Android
md Android
%CC% -o Android/rtl_wmbus rtl_wmbus.c -std=gnu99 -Iinclude -Wall -O3 -g0 -lm

rem INSTALL
%SDK_ROOT%\platform-tools\adb push Android\rtl_wmbus /mnt/sdcard/wmbus

