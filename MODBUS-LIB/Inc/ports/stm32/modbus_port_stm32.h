/*
 * modbus_port_stm32.h
 * Modbus Portability Layer - STM32 Implementation
 * 
 * This file provides STM32 HAL-specific type definitions and macros.
 * Only included when MODBUS_PLATFORM_STM32 == 1.
 */

#ifndef MODBUS_PORT_STM32_H
#define MODBUS_PORT_STM32_H

/* ============================================================================
 * INCLUDES (STM32 HAL - Platform Specific!)
 * ============================================================================
 */
#include "stm32g4xx_hal.h"      /* Основной HAL */
#include "stm32g4xx_hal_uart.h" /* UART */
#include "stm32g4xx_hal_gpio.h" /* GPIO */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONCRETE TYPE DEFINITIONS (Match abstract types in modbus_port_types.h)
 * ============================================================================
 */

/**
 * @brief STM32 concrete UART handle type
 * Wraps HAL UART_HandleTypeDef
 */
struct ModbusUartHandle
{
    UART_HandleTypeDef *handle;  /* Pointer to HAL handle */
};

/**
 * @brief STM32 concrete GPIO port type
 * Wraps HAL GPIO_TypeDef
 */
struct ModbusGpioPort
{
    GPIO_TypeDef *port;  /* Pointer to GPIO port */
};

/**
 * @brief STM32 GPIO pin type
 * Matches HAL GPIO_Pin (uint16_t)
 */
/* Already defined as uint16_t in modbus_port_types.h */

/* ============================================================================
 * HELPER MACROS FOR STM32
 * ============================================================================
 */

/**
 * @brief Create UART handle from HAL handle
 */
#define MODBUS_UART_HANDLE(hal_handle) \
    ((ModbusUartHandle_t){.handle = (hal_handle)})

/**
 * @brief Create GPIO port from HAL port
 */
#define MODBUS_GPIO_PORT(hal_port) \
    ((ModbusGpioPort_t){.port = (hal_port)})

/**
 * @brief Get HAL handle from abstract handle
 */
#define MODBUS_GET_HAL_UART(h) ((h)->handle)
#define MODBUS_GET_HAL_GPIO(p) ((p)->port)

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_PORT_STM32_H */