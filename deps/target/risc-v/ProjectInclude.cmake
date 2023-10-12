if (NOT ROOT_DIR)
    message(FATAL_ERROR "ROOT_DIR must be defined and point to the root of STK repository!")
endif()

# Base includes
include_directories(${ROOT_DIR}/deps/target/risc-v/include)

# Base definitions
#add_definitions()

# Link directories
link_directories(${ROOT_DIR}/deps/target/risc-v/src/ld)

# Device specific include file
set(_STK_DEVICE_INC "risc-v/encoding.h" CACHE STRING "_STK_DEVICE_INC" FORCE)