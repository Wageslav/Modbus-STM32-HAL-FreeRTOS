/*
 * modbus_uart_callback.c
 * Modbus Portability Layer - STM32 UART Callbacks
 * 
 * Implements HAL UART callbacks for Modbus library.
 * Only compiled when MODBUS_PLATFORM_STM32 == 1.
 */

/* ============================================================================
 * INCLUDES
 * ============================================================================
 */
#include "ports/modbus_port.h"
#include "Modbus.h"

#if MODBUS_PLATFORM_STM32 == 1

#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_uart.h"
#include "FreeRTOS.h"
#include "cmsis_os.h"

/* ============================================================================
 * UART TX COMPLETE CALLBACK
 * ============================================================================
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    /* Find matching Modbus handler */
    for (uint8_t i = 0; i < numberHandlers; i++)
    {
        if (mHandlers[i]->port->handle == huart)
        {
            /* Notify Modbus task of TX completion */
            xTaskNotifyFromISR(mHandlers[i]->myTaskModbusAHandle, 
                               0, 
                               eNoAction, 
                               &xHigherPriorityTaskWoken);
            break;
        }
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* ============================================================================
 * UART RX COMPLETE CALLBACK
 * ============================================================================
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    /* Find matching Modbus handler */
    for (uint8_t i = 0; i < numberHandlers; i++)
    {
        if (mHandlers[i]->port->handle == huart)
        {
            if (mHandlers[i]->xTypeHW == USART_HW)
            {
                /* Add byte to ring buffer */
                RingAdd(&mHandlers[i]->xBufferRX, mHandlers[i]->dataRX);
                
                /* Restart receive */
                HAL_UART_Receive_IT(huart, &mHandlers[i]->dataRX, 1);
                
                /* Reset T3.5 timer */
                xTimerResetFromISR(mHandlers[i]->xTimerT35, 
                                   &xHigherPriorityTaskWoken);
            }
            break;
        }
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* ============================================================================
 * DMA ERROR CALLBACK (if DMA enabled)
 * ============================================================================
 */
#if ENABLE_USART_DMA == 1

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    for (uint8_t i = 0; i < numberHandlers; i++)
    {
        if (mHandlers[i]->port->handle == huart)
        {
            if (mHandlers[i]->xTypeHW == USART_HW_DMA)
            {
                /* Restart DMA receive */
                while (HAL_UARTEx_ReceiveToIdle_DMA(huart, 
                        mHandlers[i]->xBufferRX.uxBuffer, 
                        MAX_BUFFER) != HAL_OK)
                {
                    HAL_UART_DMAStop(huart);
                }
                __HAL_DMA_DISABLE_IT(huart->hdmarx, DMA_IT_HT);
            }
            break;
        }
    }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    for (uint8_t i = 0; i < numberHandlers; i++)
    {
        if (mHandlers[i]->port->handle == huart)
        {
            if (mHandlers[i]->xTypeHW == USART_HW_DMA && Size > 0)
            {
                mHandlers[i]->xBufferRX.u8available = Size;
                mHandlers[i]->xBufferRX.overflow = false;
                
                /* Restart DMA */
                while (HAL_UARTEx_ReceiveToIdle_DMA(huart, 
                        mHandlers[i]->xBufferRX.uxBuffer, 
                        MAX_BUFFER) != HAL_OK)
                {
                    HAL_UART_DMAStop(huart);
                }
                __HAL_DMA_DISABLE_IT(huart->hdmarx, DMA_IT_HT);
                
                /* Notify Modbus task */
                xTaskNotifyFromISR(mHandlers[i]->myTaskModbusAHandle, 
                                   0, 
                                   eSetValueWithOverwrite, 
                                   &xHigherPriorityTaskWoken);
            }
            break;
        }
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

#endif /* ENABLE_USART_DMA */

#endif /* MODBUS_PLATFORM_STM32 */