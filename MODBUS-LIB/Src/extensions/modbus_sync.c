/*
 * modbus_sync.c
 * Modbus Synchronization Layer - FreeRTOS mutex implementation
 * 
 * Implements thread-safe mutex operations for protecting shared data
 * accessed by Modbus read/write handlers.
 */

/* ============================================================================
 * INCLUDES
 * ============================================================================
 */
#include "modbus_sync.h"

#if MODBUS_SYNC_ENABLED == 1

#include "cmsis_os.h"
#include <string.h>

/* ============================================================================
 * INTERNAL STATE
 * ============================================================================
 */

/**
 * @brief Global Modbus mutex handle
 * Static to ensure it exists for lifetime of system
 */
static osMutexId_t g_modbus_mutex = NULL;

/**
 * @brief Mutex attributes structure
 * Static to avoid stack allocation issues
 */
static osMutexAttr_t g_modbus_mutex_attr = {
    .name = "ModbusDataMutex",
    .attr_bits = osMutexRecursive | osMutexPrioInherit,
    .cb_mem = NULL,
    .cb_size = 0
};

/**
 * @brief Initialization flag
 */
static bool g_initialized = false;

#if MODBUS_DEBUG_ENABLED == 1
/**
 * @brief Statistics counters
 */
static struct {
    uint32_t take_count;
    uint32_t timeout_count;
    osThreadId_t current_holder;
} g_stats = {0, 0, NULL};
#endif

/* ============================================================================
 * INITIALIZATION
 * ============================================================================
 */

void ModbusSync_Init(void)
{
    if (g_initialized)
    {
        return; /* Already initialized */
    }
    
    g_modbus_mutex = osMutexNew(&g_modbus_mutex_attr);
    
    if (g_modbus_mutex == NULL)
    {
        /* Critical error - cannot proceed without mutex */
        /* In production, you might want to handle this gracefully */
        while (1)
        {
            /* Halt - mutex creation failed */
            /* Check FreeRTOS heap size (configTOTAL_HEAP_SIZE) */
        }
    }
    
    g_initialized = true;
    
#if MODBUS_DEBUG_ENABLED == 1
    memset(&g_stats, 0, sizeof(g_stats));
#endif
}

void ModbusSync_Deinit(void)
{
    if (!g_initialized)
    {
        return;
    }
    
    if (g_modbus_mutex != NULL)
    {
        osMutexDelete(g_modbus_mutex);
        g_modbus_mutex = NULL;
    }
    
    g_initialized = false;
}

/* ============================================================================
 * MUTEX OPERATIONS
 * ============================================================================
 */

bool ModbusSync_TakeMutex(uint32_t timeout_ms)
{
    if (!g_initialized || g_modbus_mutex == NULL)
    {
        return false; /* Not initialized */
    }
    
    /* Convert milliseconds to ticks */
    uint32_t timeout_ticks = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);
    
    osStatus_t status = osMutexAcquire(g_modbus_mutex, timeout_ticks);
    
#if MODBUS_DEBUG_ENABLED == 1
    if (status == osOK)
    {
        g_stats.take_count++;
        g_stats.current_holder = osThreadGetId();
    }
    else if (status == osErrorTimeout)
    {
        g_stats.timeout_count++;
    }
#endif
    
    return (status == osOK);
}

void ModbusSync_GiveMutex(void)
{
    if (!g_initialized || g_modbus_mutex == NULL)
    {
        return; /* Not initialized or already deleted */
    }
    
    /* Safe to call even if mutex not held - osMutexRelease handles it */
    osMutexRelease(g_modbus_mutex);
    
#if MODBUS_DEBUG_ENABLED == 1
    g_stats.current_holder = NULL;
#endif
}

bool ModbusSync_IsMutexHeld(void)
{
    if (!g_initialized || g_modbus_mutex == NULL)
    {
        return false;
    }
    
    osThreadId_t owner = osMutexGetOwner(g_modbus_mutex);
    return (owner != NULL);
}

/* ============================================================================
 * OPTIONAL: SEMAPHORE FOR EVENT SIGNALING
 * ============================================================================
 */

#if MODBUS_SYNC_SEMAPHORE_ENABLED == 1

static osSemaphoreId_t g_data_update_sem = NULL;

void ModbusSync_SignalDataUpdate(void)
{
    if (g_data_update_sem != NULL)
    {
        osSemaphoreRelease(g_data_update_sem);
    }
}

bool ModbusSync_WaitDataUpdate(uint32_t timeout_ms)
{
    if (g_data_update_sem == NULL)
    {
        return false;
    }
    
    uint32_t timeout_ticks = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);
    osStatus_t status = osSemaphoreAcquire(g_data_update_sem, timeout_ticks);
    
    return (status == osOK);
}

#endif /* MODBUS_SYNC_SEMAPHORE_ENABLED */

/* ============================================================================
 * DEBUG HELPERS
 * ============================================================================
 */

#if MODBUS_DEBUG_ENABLED == 1

void ModbusSync_GetStats(uint32_t *out_count, 
                         uint32_t *out_timeouts,
                         osThreadId_t *out_holders)
{
    if (out_count != NULL)
    {
        *out_count = g_stats.take_count;
    }
    
    if (out_timeouts != NULL)
    {
        *out_timeouts = g_stats.timeout_count;
    }
    
    if (out_holders != NULL)
    {
        *out_holders = g_stats.current_holder;
    }
}

void ModbusSync_ResetStats(void)
{
    memset(&g_stats, 0, sizeof(g_stats));
}

#endif /* MODBUS_DEBUG_ENABLED */

#else /* MODBUS_SYNC_ENABLED == 0 */

/* ============================================================================
 * STUB IMPLEMENTATION (when sync is disabled)
 * ============================================================================
 */

void ModbusSync_Init(void) { }
void ModbusSync_Deinit(void) { }
bool ModbusSync_TakeMutex(uint32_t timeout_ms) { (void)timeout_ms; return true; }
void ModbusSync_GiveMutex(void) { }
bool ModbusSync_IsMutexHeld(void) { return false; }

#if MODBUS_DEBUG_ENABLED == 1
void ModbusSync_GetStats(uint32_t *a, uint32_t *b, osThreadId_t *c) 
{ 
    if (a) *a = 0; 
    if (b) *b = 0; 
    if (c) *c = NULL; 
}
void ModbusSync_ResetStats(void) { }
#endif

#endif /* MODBUS_SYNC_ENABLED */