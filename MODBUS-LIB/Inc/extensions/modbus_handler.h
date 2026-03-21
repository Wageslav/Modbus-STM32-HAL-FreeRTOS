/*
 * modbus_handler.h
 * 
 * Modbus Handler API - Bridge between library hooks and registry
 * 
 * This module provides the integration layer that connects the Modbus library
 * callback hooks with the handler registry. It implements the onReadFlex and
 * onWriteFlex callbacks that the library calls, and dispatches them to the
 * appropriate registered handlers.
 * 
 * This is the ONLY file in the library that knows about both:
 * - The library's hook interface (modbusHandler_t)
 * - The registry interface (ModbusRegistry_Lookup)
 * 
 */

#ifndef MODBUS_HANDLER_H
#define MODBUS_HANDLER_H

/* ============================================================================
 * INCLUDES
 * ============================================================================ */

#include <stdint.h>
#include <stdbool.h>
#include "ModbusConfig.h"

/* Forward declaration - library structure */
struct modbusHandler_t;
typedef struct modbusHandler_t modbusHandler_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONFIGURATION
 * ============================================================================ */

/**
 * @brief Enable access level checking
 * 
 * When enabled, handlers verify that the requested operation is allowed
 * for the current access level. Requires application to set access level
 * via ModbusHandler_SetAccessLevel().
 */
#ifndef MODBUS_HANDLER_CHECK_ACCESS
#define MODBUS_HANDLER_CHECK_ACCESS     1
#endif

/**
 * @brief Default access level
 * 
 * Used when no explicit level is set. RELEASE = production safe.
 */
#ifndef MODBUS_HANDLER_DEFAULT_ACCESS
#define MODBUS_HANDLER_DEFAULT_ACCESS   MODBUS_ACCESS_RELEASE
#endif

/* ============================================================================
 * ACCESS LEVEL MANAGEMENT
 * ============================================================================ */

/**
 * @brief Set current access level
 * 
 * Controls which registers can be accessed. Registers with higher
 * access level requirements will be rejected.
 * 
 * @param level Access level (RELEASE, SERVICE, DEBUG)
 * 
 * @note Thread-safe: uses atomic operation
 * @note Affects all subsequent Modbus operations
 */
void ModbusHandler_SetAccessLevel(uint8_t level);

/**
 * @brief Get current access level
 * 
 * @return Current access level
 */
uint8_t ModbusHandler_GetAccessLevel(void);

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

/**
 * @brief Initialize handler bridge
 * 
 * Sets up internal state and prepares for attachment to library.
 * Must be called once before ModbusHandler_Attach().
 * 
 * @note Call this after ModbusRegistry_Init()
 */
void ModbusHandler_Init(void);

/**
 * @brief Attach handler to Modbus library instance
 * 
 * Connects the library's callback hooks to the registry lookup system.
 * After this call, all Modbus read/write requests will be dispatched
 * through the registry to application-registered handlers.
 * 
 * @param modH Pointer to modbusHandler_t instance (from library)
 * @param appContext Opaque pointer passed to handlers (optional)
 * 
 * @note Call this after ModbusInit() but before ModbusStart()
 * @note The modH pointer must remain valid for the lifetime of the system
 * 
 * @example
 * ```c
 * // In main.c:
 * ModbusInit(&ModbusH);
 * ModbusRegistry_Init();
 * ModbusApp_RegisterHandlers();  // Your registration
 * ModbusHandler_Attach(&ModbusH, NULL);
 * ModbusStart(&ModbusH);
 * ```
 */
void ModbusHandler_Attach(modbusHandler_t *modH, void *appContext);

/**
 * @brief Detach handler from Modbus library instance
 * 
 * Disconnects the callback hooks. Library falls back to direct
 * memory access (u16regs array) if available.
 * 
 * @param modH Pointer to modbusHandler_t instance
 * 
 * @note Useful for testing or dynamic reconfiguration
 */
void ModbusHandler_Detach(modbusHandler_t *modH);

/* ============================================================================
 * STATISTICS (DEBUG)
 * ============================================================================ */

#if MODBUS_DEBUG_ENABLED == 1

/**
 * @brief Get handler statistics
 */
typedef struct
{
    uint32_t read_count;        /**< Number of read operations */
    uint32_t write_count;       /**< Number of write operations */
    uint32_t read_errors;       /**< Number of read failures */
    uint32_t write_errors;      /**< Number of write failures */
    uint32_t access_denied;     /**< Number of access level rejections */
    uint32_t not_found;         /**< Number of address lookup failures */
} ModbusHandlerStats_t;

/**
 * @brief Get handler statistics
 * 
 * @param stats Pointer to statistics structure to fill
 * 
 * @note Only available when MODBUS_DEBUG_ENABLED == 1
 */
void ModbusHandler_GetStats(ModbusHandlerStats_t *stats);

/**
 * @brief Reset handler statistics
 * 
 * @note Only available when MODBUS_DEBUG_ENABLED == 1
 */
void ModbusHandler_ResetStats(void);

#endif /* MODBUS_DEBUG_ENABLED */

/* ============================================================================
 * END OF HEADER
 * ============================================================================ */

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_HANDLER_H */