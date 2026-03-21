/*
 * modbus_registry.h
 * 
 * Modbus Handler Registry - Dynamic registration of read/write handlers
 * 
 * This module provides a central registry for mapping Modbus addresses
 * to application-specific handler functions. It supports:
 * - Simple registers (U16)
 * - Packed data structures (U32, unions, custom types)
 * - Address ranges (arrays, queues)
 * - Command triggers (write-only)
 * 
 */

#ifndef MODBUS_REGISTRY_H
#define MODBUS_REGISTRY_H

/* ============================================================================
 * INCLUDES
 * ============================================================================ */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "ModbusConfig.h"

/* Include data layer types if enabled */
#if MODBUS_DATA_LAYER_ENABLED == 1
#include "modbus_data.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * REGISTER TYPE ENUMERATION
 * ============================================================================ */

/**
 * @brief Register access types
 * 
 * Defines how a register can be accessed:
 * - SIMPLE: Single 16-bit value
 * - PACKED: Multi-byte structure (U32, union, etc.)
 * - RANGE: Address range for arrays/queues
 * - CMD: Write-only command trigger
 */
typedef enum
{
    MODBUS_REG_SIMPLE = 0,    /**< Single 16-bit register */
    MODBUS_REG_PACKED = 1,    /**< Multi-byte structure */
    MODBUS_REG_RANGE = 2,     /**< Address range (array/queue) */
    MODBUS_REG_CMD = 3,       /**< Command trigger (WO) */
} ModbusRegType_t;

/* ============================================================================
 * ACCESS LEVEL ENUMERATION
 * ============================================================================ */

/**
 * @brief Access level for security
 * 
 * Defines minimum access level required to read/write:
 * - RELEASE: Production access (always allowed)
 * - SERVICE: Service/maintenance access
 * - DEBUG: Debug/development access (disabled in production)
 */
typedef enum
{
    MODBUS_ACCESS_RELEASE = 0,    /**< Production access */
    MODBUS_ACCESS_SERVICE = 1,    /**< Service access */
    MODBUS_ACCESS_DEBUG = 2,      /**< Debug access */
} ModbusAccessLevel_t;

/* ============================================================================
 * HANDLER FUNCTION TYPES
 * ============================================================================ */

#if MODBUS_DATA_LAYER_ENABLED == 1
/**
 * @brief Read handler function type
 * 
 * Called when a read request (FC3/FC4) is received for this address.
 * Handler must fill the response structure with data to send.
 * 
 * @param address Register address being read
 * @param response Pointer to response structure to fill
 * @param context User-defined context (from descriptor)
 * @return true on success, false on error
 * 
 * @note Data pointed to by response->value must remain valid after return
 * @note Use static variables or ensure data lifetime exceeds function scope
 */
typedef bool (*ModbusReadHook_t)(uint16_t address, 
                                  ModbusDataResponse_t *response, 
                                  void *context);

/**
 * @brief Write handler function type
 * 
 * Called when a write request (FC6/FC16) is received for this address.
 * Handler must parse incoming data and apply changes.
 * 
 * @param address Register address being written
 * @param request Pointer to request structure with incoming data
 * @param context User-defined context (from descriptor)
 * @return true on success, false on error (sends exception response)
 * 
 * @note Keep execution time under 1ms to avoid Modbus timeout
 * @note For heavy operations, queue work to background task
 */
typedef bool (*ModbusWriteHook_t)(uint16_t address, 
                                   const ModbusWriteRequest_t *request, 
                                   void *context);
#else
/* Legacy types without data layer */
typedef bool (*ModbusReadHook_t)(uint16_t address, uint16_t *value, void *context);
typedef bool (*ModbusWriteHook_t)(uint16_t address, uint16_t value, void *context);
#endif

/* ============================================================================
 * REGISTER DESCRIPTOR STRUCTURE
 * ============================================================================ */

/**
 * @brief Register descriptor
 * 
 * Defines a single register or address range with associated handlers.
 * Register this structure with ModbusRegistry_Register().
 */
typedef struct
{
    uint16_t address;             /**< Start address (inclusive) */
    uint16_t address_end;         /**< End address (inclusive, =address for single) */
    ModbusRegType_t type;         /**< Register type */
    ModbusAccessLevel_t access_level; /**< Minimum access level */
    
    ModbusReadHook_t read_hook;   /**< Read handler function */
    ModbusWriteHook_t write_hook; /**< Write handler function */
    void *context;                /**< User context passed to handlers */
} ModbusRegDescriptor_t;

/* ============================================================================
 * PUBLIC API - INITIALIZATION
 * ============================================================================ */

/**
 * @brief Initialize the registry
 * 
 * Clears all registered entries and resets internal state.
 * Must be called once at system startup before registering handlers.
 * 
 * @note Thread-safe: can be called before scheduler starts
 */
void ModbusRegistry_Init(void);

/**
 * @brief Clear the registry
 * 
 * Removes all registered entries. Useful for testing or reconfiguration.
 * 
 * @note Thread-safe: requires external synchronization if called after init
 */
void ModbusRegistry_Clear(void);

/* ============================================================================
 * PUBLIC API - REGISTRATION
 * ============================================================================ */

/**
 * @brief Register a single descriptor
 * 
 * Adds a register descriptor to the internal registry.
 * The descriptor is copied, so the original can be stack-allocated.
 * 
 * @param desc Pointer to descriptor structure
 * @return true on success, false on error (full, overlap, invalid)
 * 
 * @note Check return value - registration can fail if registry is full
 * @note Overlap detection is enabled by MODBUS_REGISTRY_CHECK_OVERLAP
 */
bool ModbusRegistry_Register(const ModbusRegDescriptor_t *desc);

/**
 * @brief Register an address range (convenience function)
 * 
 * Simplified registration for address ranges with common handlers.
 * 
 * @param start_addr Start address (inclusive)
 * @param end_addr End address (inclusive)
 * @param read_hook Read handler for all addresses in range
 * @param write_hook Write handler for all addresses in range
 * @param context User context passed to handlers
 * @param level Access level for the range
 * @return true on success, false on error
 */
bool ModbusRegistry_RegisterRange(uint16_t start_addr, 
                                   uint16_t end_addr,
                                   ModbusReadHook_t read_hook,
                                   ModbusWriteHook_t write_hook,
                                   void *context,
                                   ModbusAccessLevel_t level);

/**
 * @brief Register multiple descriptors from array
 * 
 * Batch registration for multiple registers.
 * 
 * @param descs Array of descriptors
 * @param count Number of descriptors in array
 * @return Number of successfully registered descriptors
 */
size_t ModbusRegistry_RegisterMany(const ModbusRegDescriptor_t *descs, 
                                    size_t count);

/* ============================================================================
 * PUBLIC API - LOOKUP
 * ============================================================================ */

/**
 * @brief Lookup descriptor by address
 * 
 * Finds the descriptor that contains the given address.
 * Returns NULL if no matching descriptor is found.
 * 
 * @param address Register address to lookup
 * @return Pointer to descriptor, or NULL if not found
 * 
 * @note Linear search - O(n) complexity
 * @note For large registries, consider hash-based lookup
 */
const ModbusRegDescriptor_t* ModbusRegistry_Lookup(uint16_t address);

/**
 * @brief Check if address is registered
 * 
 * Quick check without returning descriptor.
 * 
 * @param address Register address to check
 * @return true if address is registered, false otherwise
 */
bool ModbusRegistry_IsRegistered(uint16_t address);

/**
 * @brief Get number of registered entries
 * 
 * @return Current number of registered descriptors
 */
size_t ModbusRegistry_GetCount(void);

/**
 * @brief Get maximum registry capacity
 * 
 * @return Maximum number of entries (MAX_REGISTRY_ENTRIES)
 */
size_t ModbusRegistry_GetMaxCount(void);

/* ============================================================================
 * END OF HEADER
 * ============================================================================ */

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_REGISTRY_H */