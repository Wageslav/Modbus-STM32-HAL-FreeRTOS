/*
 * modbus_handler.h
 * Modbus Handler API - Bridge between library hooks and registry
 * 
 * This module provides the integration layer that connects the Modbus library
 * callback hooks with the handler registry. It implements the onReadFlex and
 * onWriteFlex callbacks that the library calls, and dispatches them to the
 * appropriate registered handlers.
 */

#ifndef MODBUS_HANDLER_H
#define MODBUS_HANDLER_H

/* ============================================================================
 * INCLUDES
 * ============================================================================
 */
#include <stdint.h>
#include <stdbool.h>
#include "ModbusConfig.h"
#include "Modbus.h"  /* Provides modbusHandler_t definition */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONFIGURATION
 * ============================================================================
 */
#ifndef MODBUS_HANDLER_CHECK_ACCESS
#define MODBUS_HANDLER_CHECK_ACCESS     1
#endif

#ifndef MODBUS_HANDLER_DEFAULT_ACCESS
#define MODBUS_HANDLER_DEFAULT_ACCESS   0  /* MODBUS_ACCESS_RELEASE */
#endif

/* ============================================================================
 * ACCESS LEVEL MANAGEMENT
 * ============================================================================
 */

/**
 * @brief Set current access level
 * @param level Access level (0=RELEASE, 1=SERVICE, 2=DEBUG)
 */
void ModbusHandler_SetAccessLevel(uint8_t level);

/**
 * @brief Get current access level
 * @return Current access level
 */
uint8_t ModbusHandler_GetAccessLevel(void);

/* ============================================================================
 * INITIALIZATION
 * ============================================================================
 */

/**
 * @brief Initialize handler bridge
 * @note Call this after ModbusRegistry_Init()
 */
void ModbusHandler_Init(void);

/**
 * @brief Attach handler to Modbus library instance
 * @param modH Pointer to modbusHandler_t instance (from library)
 * @param appContext Opaque pointer passed to handlers (optional)
 * @note Call this after ModbusInit() but before ModbusStart()
 */
void ModbusHandler_Attach(modbusHandler_t *modH, void *appContext);

/**
 * @brief Detach handler from Modbus library instance
 * @param modH Pointer to modbusHandler_t instance
 */
void ModbusHandler_Detach(modbusHandler_t *modH);

/* ============================================================================
 * STATISTICS (DEBUG)
 * ============================================================================
 */
#if MODBUS_DEBUG_ENABLED == 1

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
 * @param stats Pointer to statistics structure to fill
 */
void ModbusHandler_GetStats(ModbusHandlerStats_t *stats);

/**
 * @brief Reset handler statistics
 */
void ModbusHandler_ResetStats(void);

#endif /* MODBUS_DEBUG_ENABLED */

/* ============================================================================
 * PUBLIC API - LOOKUP
 * ============================================================================ */

/**
 * @brief Check if a range of registers is fully covered by registry
 * @param start_addr Starting address
 * @param num_regs Number of registers
 * @return true if all addresses are registered, false otherwise
 */
bool ModbusHandler_CheckRange(uint16_t start_addr, uint16_t num_regs);


#ifdef __cplusplus
}
#endif

#endif /* MODBUS_HANDLER_H */