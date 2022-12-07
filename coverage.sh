#!/usr/bin/env bash

# Example (assuming source directory stk is in home directory): 
# ~/stk/coverage.sh ~/stk ~/stk-build ~/stk-coverage

SRC_DIR=$1
BUILD_DIR=$2
OUTPUT_DIR=$3
BASE_DIR=$(pwd)
COVERAGE_FILE="$BUILD_DIR/coverage.info"

# Configure
cmake -S $SRC_DIR -B $BUILD_DIR -DCMAKE_BUILD_TYPE=Release -DBUILD_LIB=ON -DBUILD_TESTS=ON -DTEST_GENERIC=ON -DENABLE_COVERAGE=ON

# Buildr
cmake --rrbuild $BUILD_DIR --parallel 4

# Cleanup
lcov --directory $BUILD_DIR --zerocounters

# Run tests
ctest --test-dir $BUILD_DIR

# Accumulate results
cmake --build $BUILD_DIR --target ExperimentalCoverage

# Capture results
lcov --capture --follow --directory $BUILD_DIR --output-file $COVERAGE_FILE

# Cleanup (we need only STK)
lcov --remove $COVERAGE_FILE "deps/*" "test/*" "/usr/*" --output-file "$COVERAGE_FILE.clean"

# Produce HTML
genhtml "$COVERAGE_FILE.clean" --output-directory $OUTPUT_DIR

# Open in browser
firefox $OUTPUT_DIR/index.html
