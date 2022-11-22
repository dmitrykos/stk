add_definitions(
    -DUSE_HAL_DRIVER
    -DHSE_VALUE=8000000
    -DOS_USE_TRACE_SEMIHOSTING_DEBUG
    -DTRACE
    -DUSE_FULL_ASSERT
)

include_directories(        
    ${ROOT_DIR}/deps/target/stm32fx/include
    ${ROOT_DIR}/deps/target/stm32fx/include/arm
    ${ROOT_DIR}/deps/target/stm32fx/include/cmsis
    ${ROOT_DIR}/deps/target/stm32fx/include/cortexm
    ${ROOT_DIR}/deps/target/stm32fx/include/diag
)

if (BOARD_STM32F407DISC1)
    add_definitions(
        -DSTM32F4
        -DSTM32F407xx
    )
    include_directories(${ROOT_DIR}/deps/target/stm32fx/include/stm32f4-hal)
    link_directories(${ROOT_DIR}/deps/target/stm32fx/src/stm32f4-ld)
elseif (BOARD_STM32F0DISCOVERY)
    add_definitions(
        -DSTM32F0
        -DSTM32F051x8
    )
    include_directories(${ROOT_DIR}/deps/target/stm32fx/include/stm32f0-hal)
    link_directories(${ROOT_DIR}/deps/target/stm32fx/src/stm32f0-ld)
endif()

set(LINKER_FLAGS "-T mem.ld -T libs.ld -T sections.ld -Wl,-Map=${TARGET_NAME}.map")

set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} ${LINKER_FLAGS}")
set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} ${LINKER_FLAGS}")

list(APPEND TARGET_DEPS stm32fx)
list(APPEND TARGET_LIBS -Wl,--whole-archive stm32fx -Wl,--no-whole-archive)