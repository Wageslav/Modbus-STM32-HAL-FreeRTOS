/*
 * ModbusConfigTemplate.h
 * Modbus Library Configuration Template
 * 
 * INSTRUCTIONS:
 * 1. Copy this file to your project's Config/ directory
 * 2. Rename to ModbusConfig.h
 * 3. Adjust values according to your project needs
 * 4. Add Config/ to your include paths
 * 
 * DO NOT modify this template in the library repository.
 * Each project should have its own copy.
 */

#ifndef MODBUS_CONFIG_TEMPLATE_H
#define MODBUS_CONFIG_TEMPLATE_H

/* ============================================================================
 * CORE SETTINGS
 * ============================================================================
 */

/**
 * @brief Maximum buffer size for Modbus frames
 * Modbus RTU maximum PDU is 253 bytes. Add overhead for:
 * - Slave ID (1 byte)
 * - Function code (1 byte)
 * - CRC (2 bytes)
 * - Safety margin
 */
#define MAX_BUFFER              256

/**
 * @brief Maximum number of concurrent Modbus handlers
 * Each handler represents one Modbus channel (UART, TCP, USB-CDC).
 */
#define MAX_M_HANDLERS          4

/**
 * @brief Maximum number of telegrams in master queue
 * For master mode only. Defines queue depth for pending queries.
 */
#define MAX_TELEGRAMS           10

/**
 * @brief T3.5 timing in FreeRTOS ticks
 * Modbus RTU requires 3.5 character times of silence between frames.
 * At 115200 bps, this is approximately 0.3 ms.
 * Adjust based on your baud rate.
 */
#define T35                     pdMS_TO_TICKS(2)

/**
 * @brief Default communication timeout (milliseconds)
 * For master mode: time to wait for slave response.
 */
#define TIMEOUT_MODBUS          1000

/* ============================================================================
 * EXTENSION MODULES (ENABLE/DISABLE)
 * ============================================================================
 */

/**
 * @brief Enable flexible data layer (serialization/deserialization)
 * REQUIRED for advanced register handling with custom data types (U32, PACKED, etc.)
 */
#define MODBUS_DATA_LAYER_ENABLED   1

/**
 * @brief Enable handler registry
 * REQUIRED for project-specific handler tables with dynamic registration.
 */
#define MODBUS_REGISTRY_ENABLED     1

/**
 * @brief Enable handler API bridge
 * Connects the registry with the Modbus library hooks.
 */
#define MODBUS_HANDLER_API_ENABLED  1

/**
 * @brief Enable FreeRTOS synchronization helpers
 * Provides mutex/semaphore utilities for thread-safe register access.
 */
#define MODBUS_SYNC_ENABLED         1

/* Enable custom broadcast protocol (FC6 with 1-byte command + 3-byte mask) */
#ifndef MODBUS_CUSTOM_BROADCAST_ENABLED
#define MODBUS_CUSTOM_BROADCAST_ENABLED 1
#endif

/* ============================================================================
 * TRANSPORT LAYERS
 * ============================================================================
 */

/**
 * @brief Enable USART/UART support
 * Standard Modbus RTU over UART with interrupt-driven reception.
 */
#define ENABLE_USART            1

/**
 * @brief Enable USART DMA support
 * For high baud rates (>115200) or high CPU load scenarios.
 */
#define ENABLE_USART_DMA        0

/**
 * @brief Enable USB-CDC support
 * Modbus RTU over USB Virtual COM Port. Requires USB_DEVICE middleware.
 */
#define ENABLE_USB_CDC          0

/**
 * @brief Enable TCP/IP support
 * Modbus TCP server and client. Requires LWIP middleware.
 */
#define ENABLE_TCP              0

/**
 * @brief Number of concurrent TCP connections (if TCP enabled)
 */
#define NUMBERTCPCONN           4

/**
 * @brief TCP connection aging cycles (if TCP enabled)
 */
#define TCPAGINGCYCLES          100

/* ============================================================================
 * DEBUG SETTINGS
 * ============================================================================
 */

/**
 * @brief Enable debug output
 * When enabled, library can output debug information.
 */
#define MODBUS_DEBUG_ENABLED        1

/**
 * @brief Debug level (0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG)
 */
#define MODBUS_DEBUG_LEVEL          2

/**
 * @brief Enable address overlap detection in registry
 * Useful for debugging, adds slight overhead during registration.
 */
#define MODBUS_REGISTRY_CHECK_OVERLAP   1

/**
 * @brief Maximum registry entries
 * Adjust based on expected number of registers.
 */
#define MAX_REGISTRY_ENTRIES        256

/* ============================================================================
 * PLATFORM SPECIFIC
 * ============================================================================
 */

#define MODBUS_PLATFORM_STM32       1
#define MODBUS_PLATFORM_POSIX       0
#define MODBUS_PLATFORM_ESP32       0
#define MODBUS_USE_CMSIS_RTOS_V2    1

/* ============================================================================
 * TIMING AND TIMEOUTS
 * ============================================================================
 */

/**
 * @brief Mutex timeout for register access (milliseconds)
 * Maximum time to wait for data mutex in read/write hooks.
 */
#define MODBUS_MUTEX_TIMEOUT_MS     50

#endif /* MODBUS_CONFIG_TEMPLATE_H */