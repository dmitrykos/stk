if (ARM)
    add_custom_command(TARGET ${TARGET_BINARY}
        POST_BUILD
        COMMAND ${CMAKE_SIZE} ${TARGET_BINARY}                                 # Show size stats of the binary
        COMMAND ${CMAKE_OBJCOPY} -O ihex ${TARGET_BINARY} ${TARGET_NAME}.hex   # Export ELF to HEX additionally
        COMMAND ${CMAKE_OBJCOPY} -O binary ${TARGET_BINARY} ${TARGET_NAME}.bin # Export ELF to BIN additionally
    )
endif()