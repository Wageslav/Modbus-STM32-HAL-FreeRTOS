/*
 * ModbusConfig.h
 * 
 * Modbus Library Configuration
 * 
 * This file contains all compile-time configuration options for the Modbus library.
 * Copy this file to your project and modify according to your needs.
 * 
 */

#ifndef MODBUS_CONFIG_H
#define MODBUS_CONFIG_H

/* ============================================================================
 * CORE SETTINGS
 * ============================================================================ */

/**
 * @brief Maximum buffer size for Modbus frames
 * 
 * Modbus RTU maximum PDU is 253 bytes. Add overhead for:
 * - Slave ID (1 byte)
 * - Function code (1 byte)
 * - CRC (2 bytes)
 * - Safety margin
 */
#define MAX_BUFFER              256

/**
 * @brief Maximum number of concurrent Modbus handlers
 * 
 * Each handler represents one Modbus channel (UART, TCP, USB-CDC).
 * Limited by available hardware peripherals and FreeRTOS resources.
 */
#define MAX_M_HANDLERS          4

/**
 * @brief Maximum number of telegrams in master queue
 * 
 * For master mode only. Defines queue depth for pending queries.
 */
#define MAX_TELEGRAMS           10

/**
 * @brief T3.5 timing in FreeRTOS ticks
 * 
 * Modbus RTU requires 3.5 character times of silence between frames.
 * At 115200 bps, this is approximately 0.3 ms.
 * Adjust based on your baud rate.
 */
#define T35                     pdMS_TO_TICKS(2)

/* ============================================================================
 * EXTENSION MODULES (ENABLE/DISABLE)
 * ============================================================================ */

/**
 * @brief Enable flexible data layer (serialization/deserialization)
 * 
 * When enabled, provides ModbusDataResponse_t and ModbusWriteRequest_t
 * for type-safe register access (U16, U32, PACKED, RAW, STRUCT).
 * 
 * REQUIRED for advanced register handling with custom data types.
 */
#define MODBUS_DATA_LAYER_ENABLED   1

/**
 * @brief Enable handler registry
 * 
 * When enabled, provides ModbusRegistry_Register() and 
 * ModbusRegistry_Lookup() for dynamic handler registration.
 * 
 * REQUIRED for project-specific handler tables.
 */
#define MODBUS_REGISTRY_ENABLED     1

/**
 * @brief Enable handler API bridge
 * 
 * When enabled, provides ModbusHandler_Attach() to connect
 * the registry with the Modbus library hooks.
 */
#define MODBUS_HANDLER_API_ENABLED  1

/**
 * @brief Enable FreeRTOS synchronization helpers
 * 
 * When enabled, provides mutex/semaphore utilities for
 * thread-safe register access.
 */
#define MODBUS_SYNC_ENABLED         1

/* ============================================================================
 * TRANSPORT LAYERS
 * ============================================================================ */

/**
 * @brief Enable USART/UART support
 * 
 * Standard Modbus RTU over UART with interrupt-driven reception.
 */
#define ENABLE_USART            1

/**
 * @brief Enable USART DMA support
 * 
 * For high baud rates (>115200) or high CPU load scenarios.
 * Requires DMA channel configuration in CubeMX.
 */
#define ENABLE_USART_DMA        0

/**
 * @brief Enable USB-CDC support
 * 
 * Modbus RTU over USB Virtual COM Port.
 * Requires USB_DEVICE middleware in CubeMX.
 */
#define ENABLE_USB_CDC          0

/**
 * @brief Enable TCP/IP support
 * 
 * Modbus TCP server and client.
 * Requires LWIP middleware in CubeMX.
 */
#define ENABLE_TCP              0

/**
 * @brief Number of concurrent TCP connections
 * 
 * For TCP server mode. Defines connection pool size.
 */
#define NUMBERTCPCONN           4

/**
 * @brief TCP connection aging cycles
 * 
 * Inactive connections are closed after this many cycles.
 */
#define TCPAGINGCYCLES          100

/* ============================================================================
 * DEBUG SETTINGS
 * ============================================================================ */

/**
 * @brief Enable debug output
 * 
 * When enabled, library can output debug information via:
 * - LOG macros (if debug_log.h is available)
 * - Assert statements
 * - Error counters
 */
#define MODBUS_DEBUG_ENABLED        1

/**
 * @brief Debug level
 * 
 * 0 = NONE (no debug output)
 * 1 = ERROR (only errors)
 * 2 = WARN (warnings + errors)
 * 3 = INFO (info + warnings + errors)
 * 4 = DEBUG (full debug output)
 */
#define MODBUS_DEBUG_LEVEL          2

/**
 * @brief Enable address overlap detection
 * 
 * When enabled, registry checks for overlapping address ranges
 * during registration. Useful for debugging, adds slight overhead.
 */
#define MODBUS_REGISTRY_CHECK_OVERLAP   1

/**
 * @brief Maximum registry entries
 * 
 * Defines size of internal registry array.
 * Adjust based on expected number of registers.
 */
#define MAX_REGISTRY_ENTRIES        256

/* ============================================================================
 * PLATFORM SPECIFIC
 * ============================================================================ */

/**
 * @brief Platform type
 * 
 * Select your target platform for conditional compilation.
 */
#define MODBUS_PLATFORM_STM32       1
#define MODBUS_PLATFORM_POSIX       0

/**
 * @brief Use CMSIS-RTOS V2 API
 * 
 * Required for FreeRTOS integration with STM32Cube.
 */
#define MODBUS_USE_CMSIS_RTOS_V2    1

/* ============================================================================
 * TIMING AND TIMEOUTS
 * ============================================================================ */

/**
 * @brief Default communication timeout (milliseconds)
 * 
 * For master mode: time to wait for slave response.
 * For slave mode: not used (slave always responds).
 */
#define MODBUS_DEFAULT_TIMEOUT_MS   1000

/* ===== TIMING CONSTANTS ===== */
#ifndef TIMEOUT_MODBUS
#define TIMEOUT_MODBUS  MODBUS_DEFAULT_TIMEOUT_MS /* Default timeout: 1000 ms */
#endif

/**
 * @brief Mutex timeout for register access (milliseconds)
 * 
 * Maximum time to wait for data mutex in read/write hooks.
 * Prevents deadlocks if application task is blocked.
 */
#define MODBUS_MUTEX_TIMEOUT_MS     50

/* ============================================================================
 * END OF CONFIGURATION
 * ============================================================================ */

#endif /* MODBUS_CONFIG_H */