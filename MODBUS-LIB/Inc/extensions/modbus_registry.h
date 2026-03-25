/*
 * modbus_registry.h
 *
 * Modbus Handler Registry - Dynamic registration of read/write handlers
 */

#ifndef MODBUS_REGISTRY_H
#define MODBUS_REGISTRY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "ModbusConfig.h"

/* Include core Modbus types (ModbusResult_t, etc.) */
#include "Modbus.h"

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

typedef enum {
    MODBUS_REG_SIMPLE = 0,    /**< Single 16-bit register */
    MODBUS_REG_PACKED = 1,    /**< Multi-byte structure */
    MODBUS_REG_RANGE = 2,     /**< Address range (array/queue) */
    MODBUS_REG_CMD = 3,       /**< Command trigger (WO) */
} ModbusRegType_t;

/* ============================================================================
 * ACCESS LEVEL ENUMERATION
 * ============================================================================ */

typedef enum {
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
 * @param address Register address being read
 * @param response Pointer to response structure to fill
 * @param context User-defined context (from descriptor)
 * @return MB_RESULT_OK – send response; MB_RESULT_SILENT – no response; MB_RESULT_EXCEPTION – send exception
 */
typedef ModbusResult_t (*ModbusReadHook_t)(uint16_t address,
                                            ModbusDataResponse_t *response,
                                            void *context);

/**
 * @brief Write handler function type
 * @param address Register address being written
 * @param request Pointer to request structure with incoming data
 * @param context User-defined context (from descriptor)
 * @return MB_RESULT_OK – send confirmation; MB_RESULT_SILENT – no response; MB_RESULT_EXCEPTION – send exception
 */
typedef ModbusResult_t (*ModbusWriteHook_t)(uint16_t address,
                                             const ModbusWriteRequest_t *request,
                                             void *context);

#else
/* Legacy fallback (if data layer disabled) */
typedef bool (*ModbusReadHook_t)(uint16_t address, uint16_t *value, void *context);
typedef bool (*ModbusWriteHook_t)(uint16_t address, uint16_t value, void *context);
#endif

/* ============================================================================
 * REGISTER DESCRIPTOR STRUCTURE
 * ============================================================================ */

typedef struct {
    uint16_t address;                 /**< Start address (inclusive) */
    uint16_t address_end;             /**< End address (inclusive, =address for single) */
    ModbusRegType_t type;             /**< Register type */
    ModbusAccessLevel_t access_level; /**< Minimum access level */
    
    ModbusReadHook_t read_hook;       /**< Read handler function */
    ModbusWriteHook_t write_hook;     /**< Write handler function */
    void *context;                    /**< User context passed to handlers */
} ModbusRegDescriptor_t;

/* ============================================================================
 * PUBLIC API
 * ============================================================================ */

void ModbusRegistry_Init(void);
void ModbusRegistry_Clear(void);

bool ModbusRegistry_Register(const ModbusRegDescriptor_t *desc);
bool ModbusRegistry_RegisterRange(uint16_t start_addr,
                                   uint16_t end_addr,
                                   ModbusReadHook_t read_hook,
                                   ModbusWriteHook_t write_hook,
                                   void *context,
                                   ModbusAccessLevel_t level);
size_t ModbusRegistry_RegisterMany(const ModbusRegDescriptor_t *descs,
                                    size_t count);

const ModbusRegDescriptor_t* ModbusRegistry_Lookup(uint16_t address);
bool ModbusRegistry_IsRegistered(uint16_t address);
size_t ModbusRegistry_GetCount(void);
size_t ModbusRegistry_GetMaxCount(void);
uint16_t ModbusRegistry_GetMaxAddress(void);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_REGISTRY_H */