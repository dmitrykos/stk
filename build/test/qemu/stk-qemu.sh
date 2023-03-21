#!/usr/bin/env bash

CPU=$1
BOARD=$2
WORK_DIR=$3
KERNEL=$4

# Run emulator
docker run -v ${WORK_DIR}:/fw stk-qemu:latest qemu-system-gnuarmeclipse \
	-cpu ${CPU} -machine ${BOARD} -nographic \
	-kernel /fw/${KERNEL} > /dev/null & PID=$!

# Check if emulator started by checking its PID
ps --pid "$PID" >/dev/null
if [ "$?" -ne 0 ]; then
    echo "No pid for QEMU instance found! QEMU not started."
    exit 1
fi

# Wait for the output (semihosting)
sleep 10

# Kill emulator
kill ${PID} &> /dev/null
