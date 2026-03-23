/*
 * modbus_uart_callback.h
 * UART callback diagnostics for Modbus library
 *
 * Provides diagnostic variables to track UART receive errors.
 */

#ifndef MODBUS_UART_CALLBACK_H
#define MODBUS_UART_CALLBACK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Диагностические переменные для отслеживания ошибок UART */
extern volatile uint32_t
    modbus_uart_restart_error;                 // код ошибки HAL_UART_Receive_IT
extern volatile uint32_t modbus_uart_last_isr; // регистр ISR в момент ошибки
extern volatile uint32_t
    modbus_uart_error_count; // счётчик вызовов ErrorCallback

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_UART_CALLBACK_H */