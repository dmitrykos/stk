#!/usr/bin/env bash

CPU=$1
BOARD=$2
WORK_DIR=$3
KERNEL=$4

EMU_DIR="$(dirname -- "$(which -- "$0" 2>/dev/null || realpath -- "./$0")")"

# Run emulator
RESULT=$(${EMU_DIR}/stk-qemu.sh ${CPU} ${BOARD} ${WORK_DIR} ${KERNEL} 2>&1)

# Check if test resulted in success by checking for presence of ' STKTEST-RESULT: 0 ' string
# in the output string
if echo $RESULT | grep -q " STKTEST-RESULT: 0 "; then
   echo "SUCCESS"
   echo "$RESULT" # Show actual output from emulator
else
   echo "FAIL"    # Log failure
   echo "$RESULT" # Show actual output from emulator
   exit 1         # Notify we gailed
fi
