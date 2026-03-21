/*
 * modbus_port_stm32.c
 * Modbus Portability Layer - STM32 Implementation
 * 
 * Implements the platform interface defined in modbus_port.h
 * using STM32 HAL functions.
 */

/* ============================================================================
 * INCLUDES
 * ============================================================================
 */
#include "ports/modbus_port.h"

#if MODBUS_PLATFORM_STM32 == 1

#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_uart.h"
#include "stm32g4xx_hal_gpio.h"

/* ============================================================================
 * UART OPERATIONS
 * ============================================================================
 */

ModbusUartState_t ModbusPort_UartGetState(ModbusUartHandle_t *huart)
{
    if (huart == NULL || huart->handle == NULL)
    {
        return MODBUS_UART_STATE_ERROR;
    }
    
    HAL_UART_State_t hal_state = HAL_UART_GetState(huart->handle);
    
    /* Map HAL states to abstract states */
    switch (hal_state)
    {
        case HAL_UART_STATE_RESET:
            return MODBUS_UART_STATE_RESET;
        case HAL_UART_STATE_READY:
            return MODBUS_UART_STATE_READY;
        case HAL_UART_STATE_BUSY:
            return MODBUS_UART_STATE_BUSY;
        case HAL_UART_STATE_BUSY_TX:
            return MODBUS_UART_STATE_BUSY_TX;
        case HAL_UART_STATE_BUSY_RX:
            return MODBUS_UART_STATE_BUSY_RX;
        case HAL_UART_STATE_BUSY_TX_RX:
            return MODBUS_UART_STATE_BUSY_TX_RX;
        case HAL_UART_STATE_TIMEOUT:
            return MODBUS_UART_STATE_TIMEOUT;
        default:
            return MODBUS_UART_STATE_ERROR;
    }
}

ModbusUartStatus_t ModbusPort_UartReceive_IT(ModbusUartHandle_t *huart, 
                                              uint8_t *pData, 
                                              uint16_t Size)
{
    if (huart == NULL || huart->handle == NULL)
    {
        return MODBUS_UART_ERROR;
    }
    
    HAL_StatusTypeDef status = HAL_UART_Receive_IT(huart->handle, pData, Size);
    
    switch (status)
    {
        case HAL_OK:
            return MODBUS_UART_OK;
        case HAL_BUSY:
            return MODBUS_UART_BUSY;
        default:
            return MODBUS_UART_ERROR;
    }
}

ModbusUartStatus_t ModbusPort_UartAbortReceive_IT(ModbusUartHandle_t *huart)
{
    if (huart == NULL || huart->handle == NULL)
    {
        return MODBUS_UART_ERROR;
    }
    
    HAL_StatusTypeDef status = HAL_UART_AbortReceive_IT(huart->handle);
    
    return (status == HAL_OK) ? MODBUS_UART_OK : MODBUS_UART_ERROR;
}

ModbusUartStatus_t ModbusPort_UartTransmit_IT(ModbusUartHandle_t *huart, 
                                               uint8_t *pData, 
                                               uint16_t Size)
{
    if (huart == NULL || huart->handle == NULL)
    {
        return MODBUS_UART_ERROR;
    }
    
    HAL_StatusTypeDef status = HAL_UART_Transmit_IT(huart->handle, pData, Size);
    
    return (status == HAL_OK) ? MODBUS_UART_OK : MODBUS_UART_ERROR;
}

void ModbusPort_UartEnableTransmitter(ModbusUartHandle_t *huart)
{
    if (huart == NULL || huart->handle == NULL)
    {
        return;
    }
    
    /* STM32-specific: Enable transmitter for half-duplex */
    HAL_HalfDuplex_EnableTransmitter(huart->handle);
}

void ModbusPort_UartEnableReceiver(ModbusUartHandle_t *huart)
{
    if (huart == NULL || huart->handle == NULL)
    {
        return;
    }
    
    /* STM32-specific: Enable receiver for half-duplex */
    HAL_HalfDuplex_EnableReceiver(huart->handle);
}

/* ============================================================================
 * GPIO OPERATIONS
 * ============================================================================
 */

void ModbusPort_GpioWrite(ModbusGpioPort_t *port, 
                          ModbusGpioPin_t pin, 
                          ModbusGpioState_t state)
{
    if (port == NULL || port->port == NULL)
    {
        return;
    }
    
    GPIO_PinState pin_state = (state == MODBUS_GPIO_SET) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(port->port, pin, pin_state);
}

/* ============================================================================
 * TIMING OPERATIONS
 * ============================================================================
 */

uint32_t ModbusPort_GetTick(void)
{
    return HAL_GetTick();
}

void ModbusPort_DelayMs(uint32_t ms)
{
    HAL_Delay(ms);
}

/* ============================================================================
 * PROTECTION (Thread Safety)
 * ============================================================================
 */

uint32_t ModbusPort_EnterCritical(void)
{
    return __get_PRIMASK();
}

void ModbusPort_ExitCritical(uint32_t primask)
{
    if (primask == 0)
    {
        __enable_irq();
    }
}

#endif /* MODBUS_PLATFORM_STM32 */