/*
 * modbus_port_types.h
 * Modbus Portability Layer - Type Definitions
 * 
 * This file defines abstract types that the Modbus library uses.
 * Each platform (STM32, POSIX, ESP32, etc.) provides concrete implementations.
 * 
 * DO NOT modify this file. Platform implementations should NOT modify this.
 */

#ifndef MODBUS_PORT_TYPES_H
#define MODBUS_PORT_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * FORWARD DECLARATIONS (Platform-specific, defined in modbus_port.h)
 * ============================================================================
 */

/**
 * @brief Abstract UART handle type
 * Defined by platform implementation (e.g., UART_HandleTypeDef* for STM32)
 */
typedef struct ModbusUartHandle ModbusUartHandle_t;

/**
 * @brief Abstract GPIO port type
 * Defined by platform implementation (e.g., GPIO_TypeDef* for STM32)
 */
typedef struct ModbusGpioPort ModbusGpioPort_t;

/**
 * @brief GPIO pin number type
 */
typedef uint16_t ModbusGpioPin_t;

/* ============================================================================
 * GPIO PIN STATES
 * ============================================================================
 */
typedef enum
{
    MODBUS_GPIO_RESET = 0,
    MODBUS_GPIO_SET = 1
} ModbusGpioState_t;

/* ============================================================================
 * UART STATE ENUMERATION
 * ============================================================================
 */
typedef enum
{
    MODBUS_UART_STATE_RESET = 0,
    MODBUS_UART_STATE_READY = 1,
    MODBUS_UART_STATE_BUSY = 2,
    MODBUS_UART_STATE_BUSY_TX = 3,
    MODBUS_UART_STATE_BUSY_RX = 4,
    MODBUS_UART_STATE_BUSY_TX_RX = 5,
    MODBUS_UART_STATE_TIMEOUT = 6,
    MODBUS_UART_STATE_ERROR = 7
} ModbusUartState_t;

/* ============================================================================
 * UART STATUS CODES
 * ============================================================================
 */
typedef enum
{
    MODBUS_UART_OK = 0,
    MODBUS_UART_ERROR = 1,
    MODBUS_UART_BUSY = 2,
    MODBUS_UART_TIMEOUT = 3
} ModbusUartStatus_t;

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_PORT_TYPES_H */