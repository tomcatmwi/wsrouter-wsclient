#!/bin/bash
PLATFORM="${1:-x64}"  # default to x64 if not specified
APP_NAME="wsclient_"
SOURCE="client"
FLAGS="-DCLIENT"

clear

case "${2}" in
     router)
        SOURCE="router"
        APP_NAME="wsrouter_"
        FLAGS="-DROUTER"
        ;;
     client)
        SOURCE="client"
        APP_NAME="wsclient_"
        FLAGS="-DCLIENT"
        ;;
esac

case "$PLATFORM" in
    arm)
        COMPILER="armv7hnl-openmandriva-linux-gnueabihf-g++"
        PLATFORM_NAME="ARM"
        APP_NAME="${APP_NAME}arm"
        ;;
    arm64)
        COMPILER="aarch64-openmandriva-linux-gnu-g++"
        PLATFORM_NAME="ARM 64-bit"
        APP_NAME="${APP_NAME}arm64"
        ;;
    x64)
        COMPILER="g++"
        PLATFORM_NAME="x64"
        APP_NAME="${APP_NAME}x64"
        ;;
    x86)
        COMPILER="i686-openmandriva-linux-gnu-g++"
        PLATFORM_NAME="x86 (32-bit)"
        APP_NAME="${APP_NAME}x86"
        ;;
    freebsd)
        COMPILER="g++"
        PLATFORM_NAME="FreeBSD x64"
        APP_NAME="${APP_NAME}freebsd_x64"
        ;;        
    mips)
        COMPILER="../compilers/mipsel-linux-muslsf-cross/bin/mipsel-linux-muslsf-g++"
        PLATFORM_NAME="MIPS (32-bit)"
        APP_NAME="${APP_NAME}mips"
        ;;
    *)
        echo "Invalid platform! Options: x86, x64, freebsd, arm, arm64, mips"
        exit 1
        ;;
esac

echo Compiling $SOURCE for $PLATFORM_NAME...

$COMPILER \
    -std=c++17 \
    -Wall \
    -fmax-errors=1 \
    -o ./bin/"$APP_NAME" \
    -pthread \
    -Wno-template-id-cdtor \
    -static \
    -Icore/asio/asio/include \
    -Icore/websocketpp \
    $FLAGS \
    "ws$SOURCE.cpp" \
    ./core/utils.cpp \
    ./"$SOURCE"/*.cpp \
    &&

echo "Stripping binary..." &&
strip ./bin/"$APP_NAME" &&
echo Completed successfully!
