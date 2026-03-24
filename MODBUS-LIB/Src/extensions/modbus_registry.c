/*
 * modbus_registry.c
 * 
 * Modbus Handler Registry - Implementation
 * 
 * This module implements the central registry for mapping Modbus addresses
 * to application-specific handler functions.
 * 
 */

/* ============================================================================
 * INCLUDES
 * ============================================================================ */

#include "modbus_registry.h"
#include <string.h>

/* ============================================================================
 * INTERNAL DATA STRUCTURES
 * ============================================================================ */

/**
 * @brief Internal registry storage
 * 
 * Static array of descriptors. Size defined by MAX_REGISTRY_ENTRIES.
 */
static ModbusRegDescriptor_t g_registry[MAX_REGISTRY_ENTRIES];

/**
 * @brief Number of registered entries
 */
static size_t g_registry_count = 0;

/**
 * @brief Initialization flag
 */
static bool g_initialized = false;

/* ============================================================================
 * STATIC HELPER FUNCTIONS
 * ============================================================================ */

#if MODBUS_REGISTRY_CHECK_OVERLAP == 1
/**
 * @brief Check for address overlap
 * 
 * Validates that new descriptor doesn't overlap with existing entries.
 * 
 * @param desc Descriptor to check
 * @return true if overlap detected, false if OK
 */
static bool ModbusRegistry_CheckOverlap(const ModbusRegDescriptor_t *desc)
{
    for (size_t i = 0; i < g_registry_count; i++)
    {
        /* Check if ranges overlap */
        if ((desc->address >= g_registry[i].address && 
             desc->address <= g_registry[i].address_end) ||
            (desc->address_end >= g_registry[i].address && 
             desc->address_end <= g_registry[i].address_end) ||
            (desc->address <= g_registry[i].address && 
             desc->address_end >= g_registry[i].address_end))
        {
            return true; /* Overlap detected */
        }
    }
    return false; /* No overlap */
}
#endif

/* ============================================================================
 * PUBLIC API - INITIALIZATION
 * ============================================================================ */

void ModbusRegistry_Init(void)
{
    memset(g_registry, 0, sizeof(g_registry));
    g_registry_count = 0;
    g_initialized = true;
}

void ModbusRegistry_Clear(void)
{
    g_registry_count = 0;
    /* Don't clear memory - just reset count */
}

/* ============================================================================
 * PUBLIC API - REGISTRATION
 * ============================================================================ */

bool ModbusRegistry_Register(const ModbusRegDescriptor_t *desc)
{
    /* Validate input */
    if (desc == NULL)
    {
        return false;
    }
    
    /* Check initialization */
    if (!g_initialized)
    {
        return false;
    }
    
    /* Check capacity */
    if (g_registry_count >= MAX_REGISTRY_ENTRIES)
    {
        return false; /* Registry full */
    }
    
    /* Validate address range */
    if (desc->address > desc->address_end)
    {
        return false; /* Invalid range */
    }
    
#if MODBUS_REGISTRY_CHECK_OVERLAP == 1
    /* Check for overlap with existing entries */
    if (ModbusRegistry_CheckOverlap(desc))
    {
        return false; /* Address overlap detected */
    }
#endif
    
    /* Copy descriptor to registry */
    memcpy(&g_registry[g_registry_count], desc, sizeof(ModbusRegDescriptor_t));
    g_registry_count++;
    
    return true;
}

bool ModbusRegistry_RegisterRange(uint16_t start_addr, 
                                   uint16_t end_addr,
                                   ModbusReadHook_t read_hook,
                                   ModbusWriteHook_t write_hook,
                                   void *context,
                                   ModbusAccessLevel_t level)
{
    ModbusRegDescriptor_t desc = {
        .address = start_addr,
        .address_end = end_addr,
        .type = MODBUS_REG_RANGE,
        .access_level = level,
        .read_hook = read_hook,
        .write_hook = write_hook,
        .context = context
    };
    
    return ModbusRegistry_Register(&desc);
}

size_t ModbusRegistry_RegisterMany(const ModbusRegDescriptor_t *descs, 
                                    size_t count)
{
    size_t registered = 0;
    
    for (size_t i = 0; i < count; i++)
    {
        if (ModbusRegistry_Register(&descs[i]))
        {
            registered++;
        }
    }
    
    return registered;
}

/* ============================================================================
 * PUBLIC API - LOOKUP
 * ============================================================================ */

const ModbusRegDescriptor_t* ModbusRegistry_Lookup(uint16_t address)
{
    if (!g_initialized)
    {
        return NULL;
    }
    
    for (size_t i = 0; i < g_registry_count; i++)
    {
        if (address >= g_registry[i].address && 
            address <= g_registry[i].address_end)
        {
            return &g_registry[i];
        }
    }
    
    return NULL; /* Not found */
}

bool ModbusRegistry_IsRegistered(uint16_t address)
{
    return (ModbusRegistry_Lookup(address) != NULL);
}

size_t ModbusRegistry_GetCount(void)
{
    return g_registry_count;
}

size_t ModbusRegistry_GetMaxCount(void)
{
    return MAX_REGISTRY_ENTRIES;
}

uint16_t ModbusRegistry_GetMaxAddress(void)
{
    uint16_t max = 0;
    for (size_t i = 0; i < g_registry_count; i++)
    {
        if (g_registry[i].address_end > max)
        {
            max = g_registry[i].address_end;
        }
    }
    return max;
}

/* ============================================================================
 * END OF FILE
 * ============================================================================ */