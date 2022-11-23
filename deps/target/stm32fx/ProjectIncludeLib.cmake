if (NOT ROOT_DIR)
    message(FATAL_ERROR "ROOT_DIR must be defined and point to the root of STK repository!")
endif()

set(LINKER_FLAGS "-T mem.ld -T libs.ld -T sections.ld")

set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} ${LINKER_FLAGS}")
set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} ${LINKER_FLAGS}")

list(APPEND TARGET_DEPS stm32fx)
list(APPEND TARGET_LIBS -Wl,--whole-archive stm32fx -Wl,--no-whole-archive)