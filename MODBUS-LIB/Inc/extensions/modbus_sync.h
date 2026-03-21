/*
 * modbus_sync.h
 * Modbus Synchronization Layer - FreeRTOS mutex/semaphore helpers
 * 
 * This module provides thread-safe synchronization primitives for
 * protecting shared data accessed by Modbus handlers.
 * 
 * Usage:
 *   1. Call ModbusSync_Init() before scheduler starts
 *   2. Use ModbusSync_TakeMutex() before accessing shared data
 *   3. Use ModbusSync_GiveMutex() after accessing shared data
 * 
 * Note: This is an optional module. Enable via MODBUS_SYNC_ENABLED in ModbusConfig.h
 */

#ifndef MODBUS_SYNC_H
#define MODBUS_SYNC_H

/* ============================================================================
 * INCLUDES
 * ============================================================================
 */
#include <stdint.h>
#include <stdbool.h>
#include "ModbusConfig.h"

/* Only include FreeRTOS headers if sync is enabled */
#if MODBUS_SYNC_ENABLED == 1
#include "cmsis_os.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONFIGURATION
 * ============================================================================
 */

/**
 * @brief Default mutex timeout in milliseconds
 * Maximum time to wait for mutex acquisition.
 * Prevents deadlocks if holder task is blocked.
 */
#ifndef MODBUS_MUTEX_TIMEOUT_MS
#define MODBUS_MUTEX_TIMEOUT_MS     50
#endif

/* ============================================================================
 * INITIALIZATION
 * ============================================================================
 */

/**
 * @brief Initialize synchronization primitives
 * Creates the global Modbus mutex for data protection.
 * 
 * @note Call this ONCE before osKernelStart()
 * @note Thread-safe: can be called before scheduler starts
 * 
 * @example
 *   // In main.c, before osKernelInitialize():
 *   ModbusSync_Init();
 */
void ModbusSync_Init(void);

/**
 * @brief Deinitialize synchronization primitives
 * Deletes the Modbus mutex and frees resources.
 * 
 * @note Call this only if you need to reinitialize
 * @note Not typically needed in embedded applications
 */
void ModbusSync_Deinit(void);

/* ============================================================================
 * MUTEX OPERATIONS
 * ============================================================================
 */

/**
 * @brief Take (acquire) the Modbus mutex
 * Blocks until mutex is available or timeout expires.
 * 
 * @param timeout_ms Timeout in milliseconds
 *                   Use 0 for non-blocking attempt
 *                   Use MODBUS_MUTEX_TIMEOUT_MS for default
 * 
 * @return true  - Mutex acquired successfully
 * @return false - Timeout expired or error
 * 
 * @note Always call ModbusSync_GiveMutex() after successful take
 * @note Do not call from ISR context
 * 
 * @example
 *   if (ModbusSync_TakeMutex(MODBUS_MUTEX_TIMEOUT_MS))
 *   {
 *       // Critical section - access shared data
 *       shared_value = new_value;
 *       ModbusSync_GiveMutex();
 *   }
 *   else
 *   {
 *       // Handle timeout - data access denied
 *   }
 */
bool ModbusSync_TakeMutex(uint32_t timeout_ms);

/**
 * @brief Give (release) the Modbus mutex
 * Releases the mutex for other tasks to acquire.
 * 
 * @note Must be called by the same task that took the mutex
 * @note Do not call from ISR context
 * @note Safe to call even if mutex was not taken (no-op)
 */
void ModbusSync_GiveMutex(void);

/**
 * @brief Check if mutex is currently held
 * 
 * @return true  - Mutex is held by some task
 * @return false - Mutex is available
 * 
 * @note For debugging and monitoring only
 * @note Do not use for synchronization logic
 */
bool ModbusSync_IsMutexHeld(void);

/* ============================================================================
 * OPTIONAL: SEMAPHORE FOR EVENT SIGNALING
 * ============================================================================
 */

#if MODBUS_SYNC_SEMAPHORE_ENABLED == 1

/**
 * @brief Signal Modbus data update event
 * Notifies waiting tasks that data has changed.
 * 
 * @note Requires MODBUS_SYNC_SEMAPHORE_ENABLED = 1
 */
void ModbusSync_SignalDataUpdate(void);

/**
 * @brief Wait for Modbus data update event
 * Blocks until data update signal or timeout.
 * 
 * @param timeout_ms Timeout in milliseconds
 * @return true  - Signal received
 * @return false - Timeout expired
 */
bool ModbusSync_WaitDataUpdate(uint32_t timeout_ms);

#endif /* MODBUS_SYNC_SEMAPHORE_ENABLED */

/* ============================================================================
 * DEBUG HELPERS (IF ENABLED)
 * ============================================================================
 */

#if MODBUS_DEBUG_ENABLED == 1

/**
 * @brief Get mutex statistics
 * 
 * @param out_count     Output: Number of successful takes
 * @param out_timeouts  Output: Number of timeout failures
 * @param out_holders   Output: Current holder task (if any)
 */
void ModbusSync_GetStats(uint32_t *out_count, 
                         uint32_t *out_timeouts,
                         osThreadId_t *out_holders);

/**
 * @brief Reset mutex statistics
 */
void ModbusSync_ResetStats(void);

#endif /* MODBUS_DEBUG_ENABLED */

/* ============================================================================
 * BACKWARD COMPATIBILITY MACROS
 * ============================================================================
 */

/* Legacy function names for compatibility */
#define ModbusSync_Lock()       ModbusSync_TakeMutex(MODBUS_MUTEX_TIMEOUT_MS)
#define ModbusSync_Unlock()     ModbusSync_GiveMutex()

/* ============================================================================
 * END OF HEADER
 * ============================================================================
 */

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_SYNC_H */