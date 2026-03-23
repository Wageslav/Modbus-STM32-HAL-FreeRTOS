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
#include "Modbus.h"
#include "main.h"
#include "ports/modbus_port.h"

#if MODBUS_PLATFORM_STM32 == 1

#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_uart.h"

volatile uint32_t modbus_uart_restart_error =
    0;                                      // код ошибки HAL_UART_Receive_IT
volatile uint32_t modbus_uart_last_isr = 0; // содержимое USART1->ISR при ошибке
volatile uint32_t modbus_uart_error_count =
    0; // количество вызовов ErrorCallback

/* ============================================================================
 * UART TX COMPLETE CALLBACK
 * ============================================================================
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  /* Find matching Modbus handler */
  for (uint8_t i = 0; i < numberHandlers; i++) {
    if (mHandlers[i]->port->handle == huart) {
      /* Notify Modbus task of TX completion */
      xTaskNotifyFromISR(mHandlers[i]->myTaskModbusAHandle, 0, eNoAction,
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
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  for (uint8_t i = 0; i < numberHandlers; i++) {
    if (mHandlers[i]->port->handle == huart) {
      if (mHandlers[i]->xTypeHW == USART_HW) {
        /* Добавляем байт в кольцевой буфер */
        RingAdd(&mHandlers[i]->xBufferRX, mHandlers[i]->dataRX);

        /* Пытаемся перезапустить приём */
        HAL_StatusTypeDef status =
            HAL_UART_Receive_IT(huart, &mHandlers[i]->dataRX, 1);
        if (status != HAL_OK) {
          /* Сохраняем ошибку для вывода в основном потоке */
          modbus_uart_restart_error = status;
          modbus_uart_last_isr = huart->Instance->ISR; // регистр статуса UART
        }

        /* Сбрасываем таймер T3.5 */
        xTimerResetFromISR(mHandlers[i]->xTimerT35, &xHigherPriorityTaskWoken);
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

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  for (uint8_t i = 0; i < numberHandlers; i++) {
    if (mHandlers[i]->port->handle == huart &&
        mHandlers[i]->xTypeHW == USART_HW) {
      modbus_uart_error_count++;

      /* Очищаем флаги ошибок */
      __HAL_UART_CLEAR_OREFLAG(huart);
      __HAL_UART_CLEAR_NEFLAG(huart);
      __HAL_UART_CLEAR_FEFLAG(huart);
      __HAL_UART_CLEAR_PEFLAG(huart);

      /* Перезапускаем приём */
      HAL_UART_Receive_IT(huart, &mHandlers[i]->dataRX, 1);

      /* Можно уведомить Modbus задачу об ошибке */
      xTaskNotifyFromISR(mHandlers[i]->myTaskModbusAHandle, 0, eNoAction,
                         &xHigherPriorityTaskWoken);
      break;
    }
  }

  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  for (uint8_t i = 0; i < numberHandlers; i++) {
    if (mHandlers[i]->port->handle == huart) {
      if (mHandlers[i]->xTypeHW == USART_HW_DMA && Size > 0) {
        mHandlers[i]->xBufferRX.u8available = Size;
        mHandlers[i]->xBufferRX.overflow = false;

        /* Restart DMA */
        while (HAL_UARTEx_ReceiveToIdle_DMA(huart,
                                            mHandlers[i]->xBufferRX.uxBuffer,
                                            MAX_BUFFER) != HAL_OK) {
          HAL_UART_DMAStop(huart);
        }
        __HAL_DMA_DISABLE_IT(huart->hdmarx, DMA_IT_HT);

        /* Notify Modbus task */
        xTaskNotifyFromISR(mHandlers[i]->myTaskModbusAHandle, 0,
                           eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
      }
      break;
    }
  }

  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

#endif /* ENABLE_USART_DMA */

#endif /* MODBUS_PLATFORM_STM32 */