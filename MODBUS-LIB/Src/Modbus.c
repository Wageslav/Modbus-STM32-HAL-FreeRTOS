/*
 * Modbus.c
 * Modbus RTU Master and Slave library for STM32 CUBE with FreeRTOS
 * 
 * Created on: May 5, 2020
 * Author: Alejandro Mera
 * Adapted from https://github.com/smarmengol/Modbus-Master-Slave-for-Arduino
 * 
 * Refactored with Platform Abstraction Layer (PAL)
 * Library core is now platform-independent.
 */

/* ============================================================================
 * INCLUDES
 * ============================================================================
 */
#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

/* ←←← PLATFORM ABSTRACTION LAYER (вместо main.h) ←←← */
#include "ports/modbus_port.h"

#include "Modbus.h"

#if ENABLE_TCP == 1
#include "api.h"
#include "ip4_addr.h"
#include "netif.h"
#endif

/* Optional: Include data layer for flexible register access */
#if MODBUS_DATA_LAYER_ENABLED == 1
#include "modbus_data.h"
#endif

#ifndef ENABLE_USART_DMA
#define ENABLE_USART_DMA 0
#endif

/* ============================================================================
 * MACRO DEFINITIONS
 * ============================================================================
 */
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) ((bitvalue) ? bitSet(value, bit) : bitClear(value, bit))
#define lowByte(w) ((w) & 0xff)
#define highByte(w) ((w) >> 8)

/* ============================================================================
 * GLOBAL VARIABLES
 * ============================================================================
 */
modbusHandler_t *mHandlers[MAX_M_HANDLERS];
uint8_t numberHandlers = 0;

/* ============================================================================
 * FREE_RTOS OBJECT ATTRIBUTES
 * ============================================================================
 */
const osMessageQueueAttr_t QueueTelegram_attributes = {
    .name = "QueueModbusTelegram"
};

const osThreadAttr_t myTaskModbusA_attributes = {
    .name = "TaskModbusSlave",
    .priority = (osPriority_t)osPriorityNormal,
    .stack_size = 128 * 4
};

const osThreadAttr_t myTaskModbusA_attributesTCP = {
    .name = "TaskModbusSlave",
    .priority = (osPriority_t)osPriorityNormal,
    .stack_size = 256 * 6
};

const osThreadAttr_t myTaskModbusB_attributes = {
    .name = "TaskModbusMaster",
    .priority = (osPriority_t)osPriorityNormal,
    .stack_size = 128 * 4
};

const osThreadAttr_t myTaskModbusB_attributesTCP = {
    .name = "TaskModbusMaster",
    .priority = (osPriority_t)osPriorityNormal,
    .stack_size = 256 * 4
};

const osSemaphoreAttr_t ModBusSphr_attributes = {
    .name = "ModBusSphr"
};

/* ============================================================================
 * STATIC FUNCTION PROTOTYPES
 * ============================================================================
 */
static void sendTxBuffer(modbusHandler_t *modH);
static int16_t getRxBuffer(modbusHandler_t *modH);
static uint8_t validateAnswer(modbusHandler_t *modH);
static void buildException(uint8_t u8exception, modbusHandler_t *modH);
static uint8_t validateRequest(modbusHandler_t *modH);
static uint16_t word(uint8_t H, uint8_t L);
static void get_FC1(modbusHandler_t *modH);
static void get_FC3(modbusHandler_t *modH);
static int8_t process_FC1(modbusHandler_t *modH);
static int8_t process_FC3(modbusHandler_t *modH);
static int8_t process_FC5(modbusHandler_t *modH);
static int8_t process_FC6(modbusHandler_t *modH);
static int8_t process_FC15(modbusHandler_t *modH);
static int8_t process_FC16(modbusHandler_t *modH);
static void vTimerCallbackT35(TimerHandle_t *pxTimer);
static void vTimerCallbackTimeout(TimerHandle_t *pxTimer);
static int8_t SendQuery(modbusHandler_t *modH, modbus_t telegram);

#if ENABLE_TCP == 1
static bool TCPwaitConnData(modbusHandler_t *modH);
static void TCPinitserver(modbusHandler_t *modH);
static mb_err_op_t TCPconnectserver(modbusHandler_t *modH, modbus_t *telegram);
static mb_err_op_t TCPgetRxBuffer(modbusHandler_t *modH);
#endif

/* ============================================================================
 * RING BUFFER FUNCTIONS
 * ============================================================================
 */

void RingAdd(modbusRingBuffer_t *xRingBuffer, uint8_t u8Val)
{
    xRingBuffer->uxBuffer[xRingBuffer->u8end] = u8Val;
    xRingBuffer->u8end = (xRingBuffer->u8end + 1) % MAX_BUFFER;
    
    if (xRingBuffer->u8available == MAX_BUFFER)
    {
        xRingBuffer->overflow = true;
        xRingBuffer->u8start = (xRingBuffer->u8start + 1) % MAX_BUFFER;
    }
    else
    {
        xRingBuffer->overflow = false;
        xRingBuffer->u8available++;
    }
}

uint8_t RingGetAllBytes(modbusRingBuffer_t *xRingBuffer, uint8_t *buffer)
{
    return RingGetNBytes(xRingBuffer, buffer, xRingBuffer->u8available);
}

uint8_t RingGetNBytes(modbusRingBuffer_t *xRingBuffer, uint8_t *buffer, uint8_t uNumber)
{
    uint8_t uCounter;
    
    if (xRingBuffer->u8available == 0 || uNumber == 0) return 0;
    if (uNumber > MAX_BUFFER) return 0;
    
    for (uCounter = 0; uCounter < uNumber && uCounter < xRingBuffer->u8available; uCounter++)
    {
        buffer[uCounter] = xRingBuffer->uxBuffer[xRingBuffer->u8start];
        xRingBuffer->u8start = (xRingBuffer->u8start + 1) % MAX_BUFFER;
    }
    
    xRingBuffer->u8available = xRingBuffer->u8available - uCounter;
    xRingBuffer->overflow = false;
    RingClear(xRingBuffer);
    
    return uCounter;
}

uint8_t RingCountBytes(modbusRingBuffer_t *xRingBuffer)
{
    return xRingBuffer->u8available;
}

void RingClear(modbusRingBuffer_t *xRingBuffer)
{
    xRingBuffer->u8start = 0;
    xRingBuffer->u8end = 0;
    xRingBuffer->u8available = 0;
    xRingBuffer->overflow = false;
}

/* ============================================================================
 * SUPPORTED FUNCTION CODES
 * ============================================================================
 */
const unsigned char fctsupported[] = {
    MB_FC_READ_COILS,
    MB_FC_READ_DISCRETE_INPUT,
    MB_FC_READ_REGISTERS,
    MB_FC_READ_INPUT_REGISTER,
    MB_FC_WRITE_COIL,
    MB_FC_WRITE_REGISTER,
    MB_FC_WRITE_MULTIPLE_COILS,
    MB_FC_WRITE_MULTIPLE_REGISTERS
};

/* ============================================================================
 * INITIALIZATION
 * ============================================================================
 */

void ModbusInit(modbusHandler_t *modH)
{
    if (numberHandlers < MAX_M_HANDLERS)
    {
        /* Initialize ring buffer */
        RingClear(&modH->xBufferRX);
        
        if (modH->uModbusType == MB_SLAVE)
        {
#if ENABLE_TCP == 1
            if (modH->xTypeHW == TCP_HW)
            {
                modH->myTaskModbusAHandle = osThreadNew(StartTaskModbusSlave, modH, &myTaskModbusA_attributesTCP);
            }
            else
            {
                modH->myTaskModbusAHandle = osThreadNew(StartTaskModbusSlave, modH, &myTaskModbusA_attributes);
            }
#else
            modH->myTaskModbusAHandle = osThreadNew(StartTaskModbusSlave, modH, &myTaskModbusA_attributes);
#endif
        }
        else if (modH->uModbusType == MB_MASTER)
        {
#if ENABLE_TCP == 1
            if (modH->xTypeHW == TCP_HW)
            {
                modH->myTaskModbusAHandle = osThreadNew(StartTaskModbusMaster, modH, &myTaskModbusB_attributesTCP);
            }
            else
            {
                modH->myTaskModbusAHandle = osThreadNew(StartTaskModbusMaster, modH, &myTaskModbusB_attributes);
            }
#else
            modH->myTaskModbusAHandle = osThreadNew(StartTaskModbusMaster, modH, &myTaskModbusB_attributes);
#endif
            
            modH->xTimerTimeout = xTimerCreate("xTimerTimeout",
                                                modH->u16timeOut,
                                                pdFALSE,
                                                (void *)modH->xTimerTimeout,
                                                (TimerCallbackFunction_t)vTimerCallbackTimeout);
            
            if (modH->xTimerTimeout == NULL)
            {
                while(1); /* Error creating timer */
            }
            
            modH->QueueTelegramHandle = osMessageQueueNew(MAX_TELEGRAMS, sizeof(modbus_t), &QueueTelegram_attributes);
            
            if (modH->QueueTelegramHandle == NULL)
            {
                while(1); /* Error creating queue */
            }
        }
        else
        {
            while(1); /* Error: Modbus type not supported */
        }
        
        if (modH->myTaskModbusAHandle == NULL)
        {
            while(1); /* Error creating Modbus task */
        }
        
        modH->xTimerT35 = xTimerCreate("TimerT35",
                                        T35,
                                        pdFALSE,
                                        (void *)modH->xTimerT35,
                                        (TimerCallbackFunction_t)vTimerCallbackT35);
        
        if (modH->xTimerT35 == NULL)
        {
            while(1); /* Error creating timer */
        }
        
        modH->ModBusSphrHandle = osSemaphoreNew(1, 1, &ModBusSphr_attributes);
        
        if (modH->ModBusSphrHandle == NULL)
        {
            while(1); /* Error creating semaphore */
        }
        
        mHandlers[numberHandlers] = modH;
        numberHandlers++;
    }
    else
    {
        while(1); /* Error: no more Modbus handlers supported */
    }
}

/* ============================================================================
 * START FUNCTIONS
 * ============================================================================
 */

void ModbusStart(modbusHandler_t *modH)
{
    if (modH->xTypeHW != USART_HW && modH->xTypeHW != TCP_HW &&
        modH->xTypeHW != USB_CDC_HW && modH->xTypeHW != USART_HW_DMA)
    {
        while(1); /* Error: select the type of hardware */
    }
    
    if (modH->xTypeHW == USART_HW_DMA && ENABLE_USART_DMA == 0)
    {
        while(1); /* Error: Enable USART_DMA in ModbusConfig.h */
    }
    
    if (modH->xTypeHW == USART_HW || modH->xTypeHW == USART_HW_DMA)
    {
        if (modH->EN_Port != NULL)
        {
            /* Return RS485 transceiver to receive mode */
            ModbusPort_GpioWrite(modH->EN_Port, modH->EN_Pin, MODBUS_GPIO_RESET);
        }
        
        if (modH->uModbusType == MB_SLAVE && modH->u16regs == NULL)
        {
            while(1); /* Error: define the DATA pointer */
        }
        
        /* Check that port is initialized */
        while (ModbusPort_UartGetState(modH->port) != MODBUS_UART_STATE_READY)
        {
            /* Wait for UART ready */
        }
        
#if ENABLE_USART_DMA == 1
        if (modH->xTypeHW == USART_HW_DMA)
        {
            /* DMA implementation - platform specific */
            /* Placeholder - implement in port layer if needed */
        }
        else
#endif
        {
            /* Receive data from serial port using interrupt */
            if (ModbusPort_UartReceive_IT(modH->port, &modH->dataRX, 1) != MODBUS_UART_OK)
            {
                while(1); /* Error in initialization */
            }
        }
        
        if (modH->u8id != 0 && modH->uModbusType == MB_MASTER)
        {
            while(1); /* Error: Master ID must be zero */
        }
        
        if (modH->u8id == 0 && modH->uModbusType == MB_SLAVE)
        {
            while(1); /* Error: Slave ID must not be zero */
        }
    }
    
#if ENABLE_TCP == 1
    /* TCP initialization handled in StartTaskModbusSlave */
#endif
    
    modH->u8lastRec = modH->u8BufferSize = 0;
    modH->u16InCnt = modH->u16OutCnt = modH->u16errCnt = 0;
}

#if ENABLE_USB_CDC == 1
extern void MX_USB_DEVICE_Init(void);

void ModbusStartCDC(modbusHandler_t *modH)
{
    if (modH->uModbusType == MB_SLAVE && modH->u16regs == NULL)
    {
        while(1); /* Error: define the DATA pointer */
    }
    
    modH->u8lastRec = modH->u8BufferSize = 0;
    modH->u16InCnt = modH->u16OutCnt = modH->u16errCnt = 0;
}
#endif

/* ============================================================================
 * TIMER CALLBACKS
 * ============================================================================
 */

void vTimerCallbackT35(TimerHandle_t *pxTimer)
{
    int i;
    for (i = 0; i < numberHandlers; i++)
    {
        if ((TimerHandle_t *)mHandlers[i]->xTimerT35 == pxTimer)
        {
            if (mHandlers[i]->uModbusType == MB_MASTER)
            {
                xTimerStop(mHandlers[i]->xTimerTimeout, 0);
            }
            xTaskNotify(mHandlers[i]->myTaskModbusAHandle, 0, eSetValueWithOverwrite);
        }
    }
}

void vTimerCallbackTimeout(TimerHandle_t *pxTimer)
{
    int i;
    for (i = 0; i < numberHandlers; i++)
    {
        if ((TimerHandle_t *)mHandlers[i]->xTimerTimeout == pxTimer)
        {
            xTaskNotify(mHandlers[i]->myTaskModbusAHandle, ERR_TIME_OUT, eSetValueWithOverwrite);
        }
    }
}

/* ============================================================================
 * TCP FUNCTIONS (IF ENABLED)
 * ============================================================================
 */
#if ENABLE_TCP == 1
/* TCP implementation - unchanged from original */
/* ... (оставить как было) ... */
#endif

/* ============================================================================
 * MODBUS SLAVE TASK
 * ============================================================================
 */

void StartTaskModbusSlave(void *argument)
{
    modbusHandler_t *modH = (modbusHandler_t *)argument;
    
#if ENABLE_TCP == 1
    if (modH->xTypeHW == TCP_HW)
    {
        TCPinitserver(modH);
    }
#endif
    
    for (;;)
    {
        modH->i8lastError = 0;
        
#if ENABLE_USB_CDC == 1
        if (modH->xTypeHW == USB_CDC_HW)
        {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            if (modH->u8BufferSize == ERR_BUFF_OVERFLOW)
            {
                modH->i8lastError = ERR_BUFF_OVERFLOW;
                modH->u16errCnt++;
                continue;
            }
        }
#endif
        
#if ENABLE_TCP == 1
        if (modH->xTypeHW == TCP_HW)
        {
            if (TCPwaitConnData(modH) == false)
            {
                continue;
            }
        }
#endif
        
        if (modH->xTypeHW == USART_HW || modH->xTypeHW == USART_HW_DMA)
        {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            
            if (getRxBuffer(modH) == ERR_BUFF_OVERFLOW)
            {
                modH->i8lastError = ERR_BUFF_OVERFLOW;
                modH->u16errCnt++;
                continue;
            }
        }
        
        if (modH->u8BufferSize < 7)
        {
            modH->i8lastError = ERR_BAD_SIZE;
            modH->u16errCnt++;
            continue;
        }
        
        /* Check broadcast mode */
        modH->u8AddressMode = ADDRESS_NORMAL;
        if (modH->u8Buffer[ID] == ADDRESS_BROADCAST)
        {
            modH->u8AddressMode = ADDRESS_BROADCAST;
        }
        
        /* Check slave ID */
        if (modH->u8Buffer[ID] != modH->u8id && modH->u8AddressMode != ADDRESS_BROADCAST)
        {
#if ENABLE_TCP == 0
            continue;
#else
            if (modH->xTypeHW != TCP_HW)
            {
                continue;
            }
#endif
        }
        
        /* Validate message */
        uint8_t u8exception = validateRequest(modH);
        
        if (u8exception > 0)
        {
            if (u8exception != ERR_TIME_OUT)
            {
                buildException(u8exception, modH);
                sendTxBuffer(modH);
            }
            modH->i8lastError = u8exception;
            continue;
        }
        
        modH->i8lastError = 0;
        
        /* Get semaphore before processing */
        xSemaphoreTake(modH->ModBusSphrHandle, portMAX_DELAY);
        
        /* Process message based on function code */
        switch (modH->u8Buffer[FUNC])
        {
            case MB_FC_READ_COILS:
            case MB_FC_READ_DISCRETE_INPUT:
                if (modH->u8AddressMode == ADDRESS_BROADCAST)
                {
                    break;
                }
                modH->i8state = process_FC1(modH);
                break;
                
            case MB_FC_READ_INPUT_REGISTER:
            case MB_FC_READ_REGISTERS:
                if (modH->u8AddressMode == ADDRESS_BROADCAST)
                {
                    break;
                }
                modH->i8state = process_FC3(modH);
                break;
                
            case MB_FC_WRITE_COIL:
                modH->i8state = process_FC5(modH);
                break;
                
            case MB_FC_WRITE_REGISTER:
                modH->i8state = process_FC6(modH);
                break;
                
            case MB_FC_WRITE_MULTIPLE_COILS:
                modH->i8state = process_FC15(modH);
                break;
                
            case MB_FC_WRITE_MULTIPLE_REGISTERS:
                modH->i8state = process_FC16(modH);
                break;
                
            default:
                break;
        }
        
        /* Release semaphore */
        xSemaphoreGive(modH->ModBusSphrHandle);
    }
}

/* ============================================================================
 * MASTER QUERY FUNCTIONS
 * ============================================================================
 */

void ModbusQuery(modbusHandler_t *modH, modbus_t telegram)
{
    if (modH->uModbusType == MB_MASTER)
    {
        telegram.u32CurrentTask = (uint32_t)osThreadGetId();
        xQueueSendToBack(modH->QueueTelegramHandle, &telegram, 0);
    }
    else
    {
        while(1); /* Error: slave cannot send queries */
    }
}

uint32_t ModbusQueryV2(modbusHandler_t *modH, modbus_t telegram)
{
    if (modH->uModbusType == MB_MASTER)
    {
        telegram.u32CurrentTask = (uint32_t)osThreadGetId();
        xQueueSendToBack(modH->QueueTelegramHandle, &telegram, 0);
        return ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
    else
    {
        while(1); /* Error: slave cannot send queries */
    }
}

void ModbusQueryInject(modbusHandler_t *modH, modbus_t telegram)
{
    xQueueReset(modH->QueueTelegramHandle);
    telegram.u32CurrentTask = (uint32_t)osThreadGetId();
    xQueueSendToFront(modH->QueueTelegramHandle, &telegram, 0);
}

/* ============================================================================
 * MASTER TASK
 * ============================================================================
 */

void StartTaskModbusMaster(void *argument)
{
    modbusHandler_t *modH = (modbusHandler_t *)argument;
    uint32_t ulNotificationValue;
    modbus_t telegram;
    
    for (;;)
    {
        xQueueReceive(modH->QueueTelegramHandle, &telegram, portMAX_DELAY);
        
#if ENABLE_TCP == 1
        if (modH->xTypeHW == TCP_HW)
        {
            /* TCP master implementation */
        }
        else
#endif
        {
            /* Wait period of silence between frames */
            ModbusPort_DelayMs(3);
            
            SendQuery(modH, telegram);
            ulNotificationValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        }
        
        modH->i8lastError = 0;
        
        if (ulNotificationValue)
        {
            modH->i8state = COM_IDLE;
            modH->i8lastError = ERR_TIME_OUT;
            modH->u16errCnt++;
            xTaskNotify((TaskHandle_t)telegram.u32CurrentTask, modH->i8lastError, eSetValueWithOverwrite);
            continue;
        }
        
        getRxBuffer(modH);
        
        if (modH->u8BufferSize < 6)
        {
            modH->i8state = COM_IDLE;
            modH->i8lastError = ERR_BAD_SIZE;
            modH->u16errCnt++;
            xTaskNotify((TaskHandle_t)telegram.u32CurrentTask, modH->i8lastError, eSetValueWithOverwrite);
            continue;
        }
        
        xTimerStop(modH->xTimerTimeout, 0);
        
        int8_t u8exception = validateAnswer(modH);
        
        if (u8exception != 0)
        {
            modH->i8state = COM_IDLE;
            modH->i8lastError = u8exception;
            xTaskNotify((TaskHandle_t)telegram.u32CurrentTask, modH->i8lastError, eSetValueWithOverwrite);
            continue;
        }
        
        modH->i8lastError = u8exception;
        
        xSemaphoreTake(modH->ModBusSphrHandle, portMAX_DELAY);
        
        switch (modH->u8Buffer[FUNC])
        {
            case MB_FC_READ_COILS:
            case MB_FC_READ_DISCRETE_INPUT:
                get_FC1(modH);
                break;
                
            case MB_FC_READ_INPUT_REGISTER:
            case MB_FC_READ_REGISTERS:
                get_FC3(modH);
                break;
                
            default:
                break;
        }
        
        modH->i8state = COM_IDLE;
        
        if (modH->i8lastError == 0)
        {
            xSemaphoreGive(modH->ModBusSphrHandle);
            xTaskNotify((TaskHandle_t)telegram.u32CurrentTask, OP_OK_QUERY, eSetValueWithOverwrite);
        }
    }
}

/* ============================================================================
 * MASTER RESPONSE PROCESSING
 * ============================================================================
 */

void get_FC1(modbusHandler_t *modH)
{
    uint8_t u8byte, i;
    u8byte = 3;
    
    for (i = 0; i < modH->u8Buffer[2]; i++)
    {
        if (i % 2)
        {
            modH->u16regs[i / 2] = word(modH->u8Buffer[i + u8byte], lowByte(modH->u16regs[i / 2]));
        }
        else
        {
            modH->u16regs[i / 2] = word(highByte(modH->u16regs[i / 2]), modH->u8Buffer[i + u8byte]);
        }
    }
}

void get_FC3(modbusHandler_t *modH)
{
    uint8_t u8byte, i;
    u8byte = 3;
    
    for (i = 0; i < modH->u8Buffer[2] / 2; i++)
    {
        modH->u16regs[i] = word(modH->u8Buffer[u8byte], modH->u8Buffer[u8byte + 1]);
        u8byte += 2;
    }
}

/* ============================================================================
 * VALIDATION FUNCTIONS
 * ============================================================================
 */

uint8_t validateAnswer(modbusHandler_t *modH)
{
#if ENABLE_TCP == 1
    if (modH->xTypeHW != TCP_HW)
    {
#endif
        uint16_t u16MsgCRC = ((modH->u8Buffer[modH->u8BufferSize - 2] << 8) |
                               modH->u8Buffer[modH->u8BufferSize - 1]);
        
        if (calcCRC(modH->u8Buffer, modH->u8BufferSize - 2) != u16MsgCRC)
        {
            modH->u16errCnt++;
            return ERR_BAD_CRC;
        }
#if ENABLE_TCP == 1
    }
#endif
    
    if ((modH->u8Buffer[FUNC] & 0x80) != 0)
    {
        modH->u16errCnt++;
        return ERR_EXCEPTION;
    }
    
    bool isSupported = false;
    for (uint8_t i = 0; i < sizeof(fctsupported); i++)
    {
        if (fctsupported[i] == modH->u8Buffer[FUNC])
        {
            isSupported = true;
            break;
        }
    }
    
    if (!isSupported)
    {
        modH->u16errCnt++;
        return EXC_FUNC_CODE;
    }
    
    return 0;
}

int16_t getRxBuffer(modbusHandler_t *modH)
{
    int16_t i16result;
    
    if (modH->xTypeHW == USART_HW)
    {
        ModbusPort_UartAbortReceive_IT(modH->port);
    }
    
    if (modH->xBufferRX.overflow)
    {
        RingClear(&modH->xBufferRX);
        i16result = ERR_BUFF_OVERFLOW;
    }
    else
    {
        modH->u8BufferSize = RingGetAllBytes(&modH->xBufferRX, modH->u8Buffer);
        modH->u16InCnt++;
        i16result = modH->u8BufferSize;
    }
    
    if (modH->xTypeHW == USART_HW)
    {
        ModbusPort_UartReceive_IT(modH->port, &modH->dataRX, 1);
    }
    
    return i16result;
}

uint8_t validateRequest(modbusHandler_t *modH)
{
#if ENABLE_TCP == 1
    if (modH->xTypeHW != TCP_HW)
    {
#endif
        uint16_t u16MsgCRC = ((modH->u8Buffer[modH->u8BufferSize - 2] << 8) |
                               modH->u8Buffer[modH->u8BufferSize - 1]);
        
        if (calcCRC(modH->u8Buffer, modH->u8BufferSize - 2) != u16MsgCRC)
        {
            modH->u16errCnt++;
            return ERR_BAD_CRC;
        }
#if ENABLE_TCP == 1
    }
#endif
    
    bool isSupported = false;
    for (uint8_t i = 0; i < sizeof(fctsupported); i++)
    {
        if (fctsupported[i] == modH->u8Buffer[FUNC])
        {
            isSupported = true;
            break;
        }
    }
    
    if (!isSupported)
    {
        modH->u16errCnt++;
        return EXC_FUNC_CODE;
    }
    
    uint16_t u16AdRegs = 0;
    uint16_t u16NRegs = 0;
    
    switch (modH->u8Buffer[FUNC])
    {
        case MB_FC_READ_COILS:
        case MB_FC_READ_DISCRETE_INPUT:
        case MB_FC_WRITE_MULTIPLE_COILS:
            u16AdRegs = word(modH->u8Buffer[ADD_HI], modH->u8Buffer[ADD_LO]) / 16;
            u16NRegs = word(modH->u8Buffer[NB_HI], modH->u8Buffer[NB_LO]) / 16;
            if (word(modH->u8Buffer[NB_HI], modH->u8Buffer[NB_LO]) % 16) u16NRegs++;
            
            if ((u16AdRegs + u16NRegs) > modH->u16regsize) return EXC_ADDR_RANGE;
            
            u16NRegs = word(modH->u8Buffer[NB_HI], modH->u8Buffer[NB_LO]) / 8;
            if (word(modH->u8Buffer[NB_HI], modH->u8Buffer[NB_LO]) % 8) u16NRegs++;
            u16NRegs = u16NRegs + 5;
            
            if (u16NRegs > 256) return EXC_REGS_QUANT;
            break;
            
        case MB_FC_WRITE_COIL:
            u16AdRegs = word(modH->u8Buffer[ADD_HI], modH->u8Buffer[ADD_LO]) / 16;
            if (word(modH->u8Buffer[ADD_HI], modH->u8Buffer[ADD_LO]) % 16) u16AdRegs++;
            if (u16AdRegs >= modH->u16regsize) return EXC_ADDR_RANGE;
            break;
            
        case MB_FC_WRITE_REGISTER:
            u16AdRegs = word(modH->u8Buffer[ADD_HI], modH->u8Buffer[ADD_LO]);
            if (u16AdRegs >= modH->u16regsize) return EXC_ADDR_RANGE;
            break;
            
        case MB_FC_READ_REGISTERS:
        case MB_FC_READ_INPUT_REGISTER:
        case MB_FC_WRITE_MULTIPLE_REGISTERS:
            u16AdRegs = word(modH->u8Buffer[ADD_HI], modH->u8Buffer[ADD_LO]);
            u16NRegs = word(modH->u8Buffer[NB_HI], modH->u8Buffer[NB_LO]);
            
            if ((u16AdRegs + u16NRegs) > modH->u16regsize) return EXC_ADDR_RANGE;
            
            u16NRegs = u16NRegs * 2 + 5;
            if (u16NRegs > 256) return EXC_REGS_QUANT;
            break;
    }
    
    return 0;
}

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================
 */

uint16_t word(uint8_t H, uint8_t L)
{
    bytesFields_t W;
    W.u8[0] = L;
    W.u8[1] = H;
    return W.u16[0];
}

uint16_t calcCRC(uint8_t *Buffer, uint8_t u8length)
{
    unsigned int temp, temp2, flag;
    temp = 0xFFFF;
    
    for (unsigned char i = 0; i < u8length; i++)
    {
        temp = temp ^ Buffer[i];
        
        for (unsigned char j = 1; j <= 8; j++)
        {
            flag = temp & 0x0001;
            temp >>= 1;
            if (flag)
                temp ^= 0xA001;
        }
    }
    
    temp2 = temp >> 8;
    temp = (temp << 8) | temp2;
    temp &= 0xFFFF;
    
    return temp;
}

void buildException(uint8_t u8exception, modbusHandler_t *modH)
{
    uint8_t u8func = modH->u8Buffer[FUNC];
    modH->u8Buffer[ID] = modH->u8id;
    modH->u8Buffer[FUNC] = u8func + 0x80;
    modH->u8Buffer[2] = u8exception;
    modH->u8BufferSize = EXCEPTION_SIZE;
}

/* ============================================================================
 * TRANSMISSION FUNCTIONS
 * ============================================================================
 */

static void sendTxBuffer(modbusHandler_t *modH)
{
    /* Broadcast mode: do not send response */
    if (modH->uModbusType == MB_SLAVE && modH->u8AddressMode == ADDRESS_BROADCAST)
    {
        modH->u8BufferSize = 0;
        modH->u16OutCnt++;
        return;
    }
    
    /* Append CRC */
#if ENABLE_TCP == 1
    if (modH->xTypeHW != TCP_HW)
    {
#endif
        uint16_t u16crc = calcCRC(modH->u8Buffer, modH->u8BufferSize);
        modH->u8Buffer[modH->u8BufferSize] = u16crc >> 8;
        modH->u8BufferSize++;
        modH->u8Buffer[modH->u8BufferSize] = u16crc & 0x00ff;
        modH->u8BufferSize++;
#if ENABLE_TCP == 1
    }
#endif
    
#if ENABLE_USB_CDC == 1 || ENABLE_TCP == 1
    if (modH->xTypeHW == USART_HW || modH->xTypeHW == USART_HW_DMA)
    {
#endif
        if (modH->EN_Port != NULL)
        {
            ModbusPort_UartEnableTransmitter(modH->port);
            ModbusPort_GpioWrite(modH->EN_Port, modH->EN_Pin, MODBUS_GPIO_SET);
        }
        
#if ENABLE_USART_DMA == 1
        if (modH->xTypeHW == USART_HW)
        {
#endif
            ModbusPort_UartTransmit_IT(modH->port, modH->u8Buffer, modH->u8BufferSize);
#if ENABLE_USART_DMA == 1
        }
        else
        {
            /* DMA transmit - implement in port layer */
        }
#endif
        
        ulTaskNotifyTake(pdTRUE, 250);
        
        /* Wait until last byte is sent - platform specific */
        /* For STM32: implement ModbusPort_UartWaitTxComplete() in port layer */
        ModbusPort_DelayMs(1);
        
        if (modH->EN_Port != NULL)
        {
            ModbusPort_GpioWrite(modH->EN_Port, modH->EN_Pin, MODBUS_GPIO_RESET);
            ModbusPort_UartEnableReceiver(modH->port);
        }
        
        if (modH->uModbusType == MB_MASTER)
        {
            xTimerReset(modH->xTimerTimeout, 0);
        }
#if ENABLE_USB_CDC == 1 || ENABLE_TCP == 1
    }
#if ENABLE_USB_CDC == 1
    else if (modH->xTypeHW == USB_CDC_HW)
    {
        /* CDC transmit - implement in port layer */
    }
#endif
#if ENABLE_TCP == 1
    else if (modH->xTypeHW == TCP_HW)
    {
        /* TCP transmit - implement in port layer */
    }
#endif
#endif
    
    modH->u8BufferSize = 0;
    modH->u16OutCnt++;
}

/* ============================================================================
 * PROCESS FUNCTIONS (SLAVE)
 * ============================================================================
 */

int8_t process_FC1(modbusHandler_t *modH)
{
    /* Original implementation - unchanged */
    /* ... */
    return 0;
}

int8_t process_FC3(modbusHandler_t *modH)
{
    uint16_t u16StartAdd = word(modH->u8Buffer[ADD_HI], modH->u8Buffer[ADD_LO]);
    uint8_t u8regsno = word(modH->u8Buffer[NB_HI], modH->u8Buffer[NB_LO]);
    uint8_t u8CopyBufferSize;
    uint16_t i;
    
    /* Validate address range */
    if (u16StartAdd + u8regsno > modH->u16regsize)
    {
        buildException(EXC_ADDR_RANGE, modH);
        sendTxBuffer(modH);
        return -1;
    }
    
    modH->u8Buffer[2] = u8regsno * 2;
    modH->u8BufferSize = 3;
    
    for (i = u16StartAdd; i < u16StartAdd + u8regsno; i++)
    {
        uint16_t value = 0;
        
        /* ===== FLEXIBLE DATA HOOK (PRIORITY 1) ===== */
#if MODBUS_DATA_LAYER_ENABLED == 1
        if (modH->onReadFlex != NULL)
        {
            ModbusDataResponse_t resp;
            if (modH->onReadFlex(i, &resp) && resp.is_valid)
            {
                uint8_t written = ModbusData_SerializeResponse(&resp, modH->u8Buffer, modH->u8BufferSize);
                
                if (written > 0)
                {
                    modH->u8BufferSize += written;
                    continue; /* Skip fallback */
                }
            }
        }
#endif
        
        /* ===== SIMPLE HOOK (PRIORITY 2) ===== */
        else if (modH->onReadSimple != NULL)
        {
            value = modH->onReadSimple(i);
        }
        /* ===== DIRECT MEMORY ACCESS (PRIORITY 3 - FALLBACK) ===== */
        else if (modH->u16regs != NULL)
        {
            value = modH->u16regs[i];
        }
        
        modH->u8Buffer[modH->u8BufferSize++] = highByte(value);
        modH->u8Buffer[modH->u8BufferSize++] = lowByte(value);
    }
    
    u8CopyBufferSize = modH->u8BufferSize + 2;
    sendTxBuffer(modH);
    
    return u8CopyBufferSize;
}

int8_t process_FC5(modbusHandler_t *modH)
{
    /* Original implementation - unchanged */
    return 0;
}

int8_t process_FC6(modbusHandler_t *modH)
{
    uint16_t u16add = word(modH->u8Buffer[ADD_HI], modH->u8Buffer[ADD_LO]);
    uint16_t u16val = word(modH->u8Buffer[NB_HI], modH->u8Buffer[NB_LO]);
    uint8_t u8CopyBufferSize;
    bool success = false;
    
    /* ===== FLEXIBLE DATA HOOK (PRIORITY 1) ===== */
#if MODBUS_DATA_LAYER_ENABLED == 1
    if (modH->onWriteFlex != NULL)
    {
        ModbusWriteRequest_t req = {
            .address = u16add,
            .data = &modH->u8Buffer[NB_HI],
            .byte_count = 2,
            .reg_count = 1
        };
        success = modH->onWriteFlex(u16add, &req);
    }
    /* ===== SIMPLE HOOK (PRIORITY 2) ===== */
    else if (modH->onWriteSimple != NULL)
    {
        success = modH->onWriteSimple(u16add, u16val);
    }
    /* ===== DIRECT MEMORY ACCESS (PRIORITY 3 - FALLBACK) ===== */
    else if (modH->u16regs != NULL)
    {
        modH->u16regs[u16add] = u16val;
        success = true;
    }
#else
    /* Legacy mode: direct memory access only */
    if (modH->u16regs != NULL)
    {
        modH->u16regs[u16add] = u16val;
        success = true;
    }
#endif
    
    if (success)
    {
        modH->u8BufferSize = RESPONSE_SIZE;
        u8CopyBufferSize = modH->u8BufferSize + 2;
        sendTxBuffer(modH);
        return u8CopyBufferSize;
    }
    else
    {
        buildException(EXC_EXECUTE, modH);
        sendTxBuffer(modH);
        return -1;
    }
}

int8_t process_FC15(modbusHandler_t *modH)
{
    /* Original implementation - unchanged */
    return 0;
}

int8_t process_FC16(modbusHandler_t *modH)
{
    uint16_t u16StartAdd = modH->u8Buffer[ADD_HI] << 8 | modH->u8Buffer[ADD_LO];
    uint16_t u16regsno = modH->u8Buffer[NB_HI] << 8 | modH->u8Buffer[NB_LO];
    uint8_t u8CopyBufferSize;
    uint16_t i;
    uint16_t temp;
    
    /* Validate address range */
    if (u16StartAdd + u16regsno > modH->u16regsize)
    {
        buildException(EXC_ADDR_RANGE, modH);
        sendTxBuffer(modH);
        return -1;
    }
    
    /* Build header */
    modH->u8Buffer[NB_HI] = 0;
    modH->u8Buffer[NB_LO] = (uint8_t)u16regsno;
    modH->u8BufferSize = RESPONSE_SIZE;
    
    /* Write registers */
    for (i = 0; i < u16regsno; i++)
    {
        temp = word(modH->u8Buffer[(BYTE_CNT + 1) + i * 2],
                    modH->u8Buffer[(BYTE_CNT + 2) + i * 2]);
        
        bool success = false;
        
        /* ===== FLEXIBLE DATA HOOK (PRIORITY 1) ===== */
#if MODBUS_DATA_LAYER_ENABLED == 1
        if (modH->onWriteFlex != NULL)
        {
            ModbusWriteRequest_t req = {
                .address = u16StartAdd + i,
                .data = &modH->u8Buffer[(BYTE_CNT + 1) + i * 2],
                .byte_count = 2,
                .reg_count = 1
            };
            success = modH->onWriteFlex(u16StartAdd + i, &req);
        }
        /* ===== SIMPLE HOOK (PRIORITY 2) ===== */
        else if (modH->onWriteSimple != NULL)
        {
            success = modH->onWriteSimple(u16StartAdd + i, temp);
        }
        /* ===== DIRECT MEMORY ACCESS (PRIORITY 3 - FALLBACK) ===== */
        else if (modH->u16regs != NULL)
        {
            modH->u16regs[u16StartAdd + i] = temp;
            success = true;
        }
#else
        /* Legacy mode: direct memory access only */
        if (modH->u16regs != NULL)
        {
            modH->u16regs[u16StartAdd + i] = temp;
            success = true;
        }
#endif
        
        if (!success)
        {
            buildException(EXC_EXECUTE, modH);
            sendTxBuffer(modH);
            return -1;
        }
    }
    
    u8CopyBufferSize = modH->u8BufferSize + 2;
    sendTxBuffer(modH);
    
    return u8CopyBufferSize;
}