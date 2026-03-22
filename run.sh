#!/usr/bin/env bash
set -e

PROJECT_DIR="$HOME/Projekti/RTOS"
DRIVER_DIR="$PROJECT_DIR/cpu_driver"
USERSPACE_DIR="$PROJECT_DIR/userspace"

CLOCK_SPEED=""

if [ $# -ge 1 ] && [[ "$1" =~ ^[0-9]+$ ]]; then
    CLOCK_SPEED="$1"
    shift
fi

echo "Removing old module if loaded..."
sudo rmmod cpu_emulator_driver 2>/dev/null || true

if [ -n "$CLOCK_SPEED" ]; then
    echo "Loading kernel module with clock_speed set to ${CLOCK_SPEED}ms"
    sudo insmod "$DRIVER_DIR/cpu_emulator_driver.ko" clock_speed="$CLOCK_SPEED"
else
    echo "Loading kernel module with clock_speed set to 500ms"
    sudo insmod "$DRIVER_DIR/cpu_emulator_driver.ko"
fi

echo "Running userspace program..."
cd "$USERSPACE_DIR"
./bin/Release/main "$@"
