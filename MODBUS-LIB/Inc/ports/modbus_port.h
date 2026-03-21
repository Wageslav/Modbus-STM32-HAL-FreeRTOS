/*
 * modbus_port.h
 * Modbus Portability Layer - Platform Interface
 * 
 * This file defines the interface that the Modbus library expects from
 * the underlying platform. Each platform (STM32, POSIX, ESP32, etc.)
 * must implement these functions.
 * 
 * The core library includes ONLY this file - never platform-specific headers.
 */

#ifndef MODBUS_PORT_H
#define MODBUS_PORT_H

/* ============================================================================
 * INCLUDES
 * ============================================================================
 */
#include "ModbusConfig.h"
#include "modbus_port_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * PLATFORM SELECTION
 * ============================================================================
 */
#ifndef MODBUS_PLATFORM_STM32
#define MODBUS_PLATFORM_STM32 0
#endif

#ifndef MODBUS_PLATFORM_POSIX
#define MODBUS_PLATFORM_POSIX 0
#endif

#ifndef MODBUS_PLATFORM_ESP32
#define MODBUS_PLATFORM_ESP32 0
#endif

/* ============================================================================
 * PLATFORM-SPECIFIC TYPE DEFINITIONS
 * ============================================================================
 */
#if MODBUS_PLATFORM_STM32 == 1
#include "ports/stm32/modbus_port_stm32.h"
#elif MODBUS_PLATFORM_POSIX == 1
#include "ports/posix/modbus_port_posix.h"
#elif MODBUS_PLATFORM_ESP32 == 1
#include "ports/esp32/modbus_port_esp32.h"
#else
#error "No platform selected. Define MODBUS_PLATFORM_* in ModbusConfig.h"
#endif

/* ============================================================================
 * UART OPERATIONS (Required for USART_HW)
 * ============================================================================
 */

/**
 * @brief Get UART state
 * @param huart Abstract UART handle
 * @return Current UART state
 */
ModbusUartState_t ModbusPort_UartGetState(ModbusUartHandle_t *huart);

/**
 * @brief Start UART receive (interrupt mode)
 * @param huart Abstract UART handle
 * @param pData Pointer to data buffer
 * @param Size Number of bytes to receive
 * @return MODBUS_UART_OK on success
 */
ModbusUartStatus_t ModbusPort_UartReceive_IT(ModbusUartHandle_t *huart, 
                                              uint8_t *pData, 
                                              uint16_t Size);

/**
 * @brief Abort UART receive (interrupt mode)
 * @param huart Abstract UART handle
 * @return MODBUS_UART_OK on success
 */
ModbusUartStatus_t ModbusPort_UartAbortReceive_IT(ModbusUartHandle_t *huart);

/**
 * @brief Start UART transmit (interrupt mode)
 * @param huart Abstract UART handle
 * @param pData Pointer to data buffer
 * @param Size Number of bytes to transmit
 * @return MODBUS_UART_OK on success
 */
ModbusUartStatus_t ModbusPort_UartTransmit_IT(ModbusUartHandle_t *huart, 
                                               uint8_t *pData, 
                                               uint16_t Size);


/* ============================================================================
 * GPIO OPERATIONS (Required for RS485 direction control)
 * ============================================================================
 */

/**
 * @brief Write GPIO pin
 * @param port Abstract GPIO port
 * @param pin GPIO pin number
 * @param state Pin state (SET/RESET)
 */
void ModbusPort_GpioWrite(ModbusGpioPort_t *port, 
                          ModbusGpioPin_t pin, 
                          ModbusGpioState_t state);

/* ============================================================================
 * TIMING OPERATIONS
 * ============================================================================
 */

/**
 * @brief Get current tick count (milliseconds)
 * @return Current tick count
 */
uint32_t ModbusPort_GetTick(void);

/**
 * @brief Delay in milliseconds
 * @param ms Number of milliseconds
 */
void ModbusPort_DelayMs(uint32_t ms);

/* ============================================================================
 * PROTECTION (Thread Safety)
 * ============================================================================
 */

/**
 * @brief Enter critical section (disable interrupts)
 * @return Previous interrupt state (for restore)
 */
uint32_t ModbusPort_EnterCritical(void);

/**
 * @brief Exit critical section (restore interrupts)
 * @param primask Previous interrupt state
 */
void ModbusPort_ExitCritical(uint32_t primask);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_PORT_H */