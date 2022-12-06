# Deps
list(APPEND TARGET_DEPS stk)
list(APPEND TARGET_LIBS stk)

# Target
if (VENDOR_STM32)
    include(${ROOT_DIR}/deps/target/stm32fx/ProjectInclude.cmake)
    include(${ROOT_DIR}/deps/target/stm32fx/ProjectIncludeLib.cmake)
    set(LINKER_FLAGS "-Wl,-Map=${TARGET_NAME}.map")
    set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} ${LINKER_FLAGS}")
    set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} ${LINKER_FLAGS}")
    set(TARGET_BINARY ${TARGET_NAME}.elf)
else()
    set(TARGET_BINARY ${TARGET_NAME})
endif()

# Make target
add_executable(${TARGET_BINARY} ${TEST_SRC})
include(${ROOT_DIR}/deps/target/PostBuild.cmake)

# Link deps
add_dependencies(${TARGET_BINARY} ${TARGET_DEPS})
target_link_libraries(${TARGET_BINARY} ${TARGET_LIBS})

# Tests
if (BUILD_TESTS)
    # Add into tests
    add_test(NAME ${TARGET_BINARY} COMMAND ${TARGET_NAME})

    # Generate test coverage report
    if (GCC AND NOT CMAKE_CROSSCOMPILING)
        target_compile_options(${TARGET_BINARY} PUBLIC "-fprofile-arcs" "-ftest-coverage")
        target_link_libraries(${TARGET_BINARY} gcov)
    endif()
endif()

install(TARGETS ${TARGET_BINARY} DESTINATION ${LIBRARY_OUTPUT_PATH})