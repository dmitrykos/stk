#!/usr/bin/env bash

# Example (assuming source directory stk is in home directory): 
# ~/stk/coverage.sh -s ~/stk -b ~/stk-build -o ~/stk-coverage

SRC_DIR=""
BUILD_DIR=""
OUTPUT_DIR=""
SHOW_IN_WEBBROWSER=false

ShowHelp()
{
   echo "Generate Coverage HTML report with gcov/lcov."
   echo
   echo "Syntax: coverage [-h|s|b|o|w]"
   echo "options:"
   echo "h       Show help."
   echo "s DIR   Source directory."
   echo "b DIR   Build directory."
   echo "o DIR   Output directory (location of HTML report)."
   echo "w       Open generated HTML report in the FireFox web browser."
   echo
}

CheckArgs()
{
    if [ -z "$SRC_DIR" ] ; then
        echo "Failed: no source directory provided!" && exit;
    fi
    if [ -z "$BUILD_DIR" ] ; then
        echo "Failed: no build directory provided!" && exit;
    fi
    if [ -z "$OUTPUT_DIR" ] ; then
        echo "Failed: no output directory provided!" && exit;
    fi
}

while getopts ":hs:b:o:w" option; do
    case $option in
        h) ShowHelp && exit;;
        s) SRC_DIR=$OPTARG;;
        b) BUILD_DIR=$OPTARG;;
        o) OUTPUT_DIR=$OPTARG;;
        w) SHOW_IN_WEBBROWSER=true;;
        \?) echo "Failed: invalid option!" && exit;;
    esac
done

CheckArgs

COVERAGE_FILE="$BUILD_DIR/coverage.info"

# Configure
cmake -S $SRC_DIR -B $BUILD_DIR -DCMAKE_BUILD_TYPE=Release -DBUILD_LIB=ON -DBUILD_TESTS=ON -DTEST_GENERIC=ON -DENABLE_COVERAGE=ON

# Buildr
cmake --build $BUILD_DIR --config Release --parallel 4

# Cleanup
lcov --directory $BUILD_DIR --zerocounters

# Run tests
ctest --test-dir $BUILD_DIR -C Release

# Accumulate results
cmake --build $BUILD_DIR --target ExperimentalCoverage

# Capture results
lcov --capture --follow --directory $BUILD_DIR --output-file $COVERAGE_FILE

# Cleanup (we need only STK)
lcov --remove $COVERAGE_FILE "deps/*" "test/*" "/usr/*" --output-file "$COVERAGE_FILE.clean"

# Produce HTML
genhtml "$COVERAGE_FILE.clean" --output-directory $OUTPUT_DIR

# Open in browser
if $SHOW_IN_WEBBROWSER ; then
    firefox $OUTPUT_DIR/index.html
fi
