add_library(Modbus STATIC
    MODBUS-LIB/Src/Modbus.c
    MODBUS-LIB/Src/extensions/modbus_data.c
    MODBUS-LIB/Src/extensions/modbus_handler.c
    MODBUS-LIB/Src/extensions/modbus_registry.c
    MODBUS-LIB/Src/extensions/modbus_sync.c
    MODBUS-LIB/Src/ports/stm32/modbus_port_stm32.c
    MODBUS-LIB/Src/ports/stm32/modbus_uart_callback.c
)

target_include_directories(Modbus PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/MODBUS-LIB/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/MODBUS-LIB/Inc/extensions
    ${CMAKE_CURRENT_SOURCE_DIR}/MODBUS-LIB/Inc/ports
    ${CMAKE_CURRENT_SOURCE_DIR}/MODBUS-LIB/Inc/ports/stm32
    ${CMAKE_SOURCE_DIR}/Config
)

target_link_libraries(Modbus PUBLIC
    stm32cubemx
)