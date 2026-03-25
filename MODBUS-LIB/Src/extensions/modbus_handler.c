/*
 * modbus_handler.c
 * 
 * Modbus Handler API - Implementation
 * 
 * This module implements the bridge between the Modbus library callback hooks
 * and the handler registry. It provides the onReadFlex and onWriteFlex
 * callback implementations that the library calls.
 * 
 * Flow:
 * 1. Library receives Modbus request (FC3/FC6/FC16)
 * 2. Library calls modH->onReadFlex() or modH->onWriteFlex()
 * 3. This module looks up address in registry
 * 4. Registry returns descriptor with handler function
 * 5. This module calls the handler with proper parameters
 * 6. Result is returned to library for response generation
 * 
 */

/* ============================================================================
 * INCLUDES
 * ============================================================================ */

#include "modbus_handler.h"
#include "modbus_registry.h"
#include "modbus_data.h"
#include "Modbus.h"
#include <string.h>

/* ============================================================================
 * INTERNAL STATE
 * ============================================================================ */

/**
 * @brief Current access level
 * 
 * Controls which registers can be accessed. Initialized to RELEASE.
 */
static volatile uint8_t g_access_level = MODBUS_ACCESS_RELEASE;

/**
 * @brief Application context pointer
 * 
 * Passed to all handler callbacks for project-specific data.
 */
static void *g_app_context = NULL;

/**
 * @brief Attachment flag
 */
static bool g_attached = false;

#if MODBUS_DEBUG_ENABLED == 1
/**
 * @brief Handler statistics
 */
static ModbusHandlerStats_t g_stats = {0};

#define STAT_INC_READ()     g_stats.read_count++
#define STAT_INC_WRITE()    g_stats.write_count++
#define STAT_INC_READ_ERR() g_stats.read_errors++
#define STAT_INC_WRITE_ERR() g_stats.write_errors++
#define STAT_INC_ACCESS()   g_stats.access_denied++
#define STAT_INC_NOTFOUND() g_stats.not_found++
#else
#define STAT_INC_READ()
#define STAT_INC_WRITE()
#define STAT_INC_READ_ERR()
#define STAT_INC_WRITE_ERR()
#define STAT_INC_ACCESS()
#define STAT_INC_NOTFOUND()
#endif

/* ============================================================================
 * ACCESS LEVEL MANAGEMENT
 * ============================================================================ */

void ModbusHandler_SetAccessLevel(uint8_t level)
{
    g_access_level = level;
}

uint8_t ModbusHandler_GetAccessLevel(void)
{
    return g_access_level;
}

/* ============================================================================
 * STATIC HELPER FUNCTIONS
 * ============================================================================ */

/**
 * @brief Check access level permission
 * 
 * @param required_level Minimum level required for this register
 * @return true if access allowed, false if denied
 */
static inline bool ModbusHandler_CheckAccess(ModbusAccessLevel_t required_level)
{
#if MODBUS_HANDLER_CHECK_ACCESS == 1
    return (g_access_level >= required_level);
#else
    (void)required_level;
    return true;
#endif
}

/* ============================================================================
 * READ HOOK IMPLEMENTATION
 * ============================================================================ */

/**
 * @brief Library read callback implementation
 * 
 * This function is called by the Modbus library when a read request
 * (FC3 or FC4) is received. It looks up the address in the registry
 * and calls the registered read handler.
 * 
 * @param address Register address being read
 * @param response Pointer to response structure to fill
 * @return true on success, false on error
 */
static bool ModbusHandler_ReadHook(uint16_t address, ModbusDataResponse_t *response)
{
    STAT_INC_READ();
    
    /* Validate input */
    if (response == NULL)
    {
        STAT_INC_READ_ERR();
        return false;
    }
    
    /* Initialize response as invalid */
    ModbusResp_SetInvalid(response);
    
    /* Lookup address in registry */
    const ModbusRegDescriptor_t *desc = ModbusRegistry_Lookup(address);
    
    if (desc == NULL)
    {
        /* Address not registered */
        STAT_INC_NOTFOUND();
        return false;
    }
    
    /* Check access level */
    if (!ModbusHandler_CheckAccess(desc->access_level))
    {
        /* Access denied - insufficient privilege level */
        STAT_INC_ACCESS();
        return false;
    }
    
    /* Check if read handler is registered */
    if (desc->read_hook == NULL)
    {
        /* No read handler - register is write-only */
        STAT_INC_READ_ERR();
        return false;
    }
    
    /* Call the registered read handler */
    bool success = desc->read_hook(address, response, desc->context);
    
    if (!success)
    {
        STAT_INC_READ_ERR();
        return false;
    }
    
    /* Validate response */
    if (!response->is_valid)
    {
        STAT_INC_READ_ERR();
        return false;
    }
    
    return true;
}

/* ============================================================================
 * WRITE HOOK IMPLEMENTATION
 * ============================================================================ */

/**
 * @brief Library write callback implementation
 * 
 * This function is called by the Modbus library when a write request
 * (FC6 or FC16) is received. It looks up the address in the registry
 * and calls the registered write handler.
 * 
 * @param address Register address being written
 * @param request Pointer to request structure with incoming data
 * @return true on success, false on error
 */
static bool ModbusHandler_WriteHook(uint16_t address, const ModbusWriteRequest_t *request)
{
    STAT_INC_WRITE();
    
    /* Validate input */
    if (request == NULL)
    {
        STAT_INC_WRITE_ERR();
        return false;
    }
    
    /* Lookup address in registry */
    const ModbusRegDescriptor_t *desc = ModbusRegistry_Lookup(address);
    
    if (desc == NULL)
    {
        /* Address not registered */
        STAT_INC_NOTFOUND();
        return false;
    }
    
    /* Check access level */
    if (!ModbusHandler_CheckAccess(desc->access_level))
    {
        /* Access denied - insufficient privilege level */
        STAT_INC_ACCESS();
        return false;
    }
    
    /* Check if write handler is registered */
    if (desc->write_hook == NULL)
    {
        /* No write handler - register is read-only */
        STAT_INC_WRITE_ERR();
        return false;
    }
    
    /* Call the registered write handler */
    bool success = desc->write_hook(address, request, desc->context);
    
    if (!success)
    {
        STAT_INC_WRITE_ERR();
        return false;
    }
    
    return true;
}

/* ============================================================================
 * PUBLIC API - INITIALIZATION
 * ============================================================================ */

void ModbusHandler_Init(void)
{
    g_access_level = MODBUS_HANDLER_DEFAULT_ACCESS;
    g_app_context = NULL;
    g_attached = false;
    
#if MODBUS_DEBUG_ENABLED == 1
    memset(&g_stats, 0, sizeof(g_stats));
#endif
}

void ModbusHandler_Attach(modbusHandler_t *modH, void *appContext)
{
    if (modH == NULL)
    {
        return;
    }
    
    /* Store application context for handlers */
    g_app_context = appContext;
    
    /* Attach our hook implementations to the library */
    modH->onReadFlex = ModbusHandler_ReadHook;
    modH->onWriteFlex = ModbusHandler_WriteHook;
    
    /* Store context in library structure for handler access */
    modH->appContext = appContext;
    
    /* Enable dynamic handler mode – ignore u16regs */
    modH->dynamic_handlers = true;
    
    g_attached = true;
}


void ModbusHandler_Detach(modbusHandler_t *modH)
{
    if (modH == NULL)
    {
        return;
    }
    
    /* Clear hooks - library falls back to u16regs */
    modH->onReadFlex = NULL;
    modH->onWriteFlex = NULL;
    modH->appContext = NULL;
    
    g_attached = false;
}

/* ============================================================================
 * PUBLIC API - STATISTICS (DEBUG)
 * ============================================================================ */

#if MODBUS_DEBUG_ENABLED == 1

void ModbusHandler_GetStats(ModbusHandlerStats_t *stats)
{
    if (stats == NULL)
    {
        return;
    }
    
    /* Copy statistics atomically */
    memcpy(stats, &g_stats, sizeof(ModbusHandlerStats_t));
}

void ModbusHandler_ResetStats(void)
{
    memset(&g_stats, 0, sizeof(g_stats));
}

#endif /* MODBUS_DEBUG_ENABLED */

/* modbus_handler.c – исправленный фрагмент */

bool ModbusHandler_CheckRange(uint16_t start_addr, uint16_t num_regs)
{
    for (uint16_t i = 0; i < num_regs; i++)
    {
        if (!ModbusRegistry_IsRegistered(start_addr + i))
        {
            return false;
        }
    }
    return true;
}

/* ============================================================================
 * END OF FILE
 * ============================================================================ */