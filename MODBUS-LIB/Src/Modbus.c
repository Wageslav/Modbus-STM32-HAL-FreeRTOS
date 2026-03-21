/*
 * Modbus.c
 *  Modbus RTU Master and Slave library for STM32 CUBE with FreeRTOS
 *  Created on: May 5, 2020
 *      Author: Alejandro Mera
 *      Adapted from https://github.com/smarmengol/Modbus-Master-Slave-for-Arduino
 */

/* ============================================================================
 * INCLUDES
 * ============================================================================ */

#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "task.h"
#include "queue.h"
#include "main.h"
#include "Modbus.h"
#include "timers.h"
#include "semphr.h"

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
 * ============================================================================ */

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) ((bitvalue) ? bitSet(value, bit) : bitClear(value, bit))
#define lowByte(w) ((w) & 0xff)
#define highByte(w) ((w) >> 8)

/* ============================================================================
 * GLOBAL VARIABLES
 * ============================================================================ */

modbusHandler_t *mHandlers[MAX_M_HANDLERS];
uint8_t numberHandlers = 0;

/* ============================================================================
 * FREE_RTOS OBJECT ATTRIBUTES
 * ============================================================================ */

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
 * ============================================================================ */

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
 * ============================================================================ */

/**
 * @brief Add byte to ring buffer
 * 
 * @note Must be called only after disabling USART RX interrupt or inside RX ISR
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

/**
 * @brief Get all available bytes from ring buffer
 * 
 * @note Must be called only after disabling USART RX interrupt
 */
uint8_t RingGetAllBytes(modbusRingBuffer_t *xRingBuffer, uint8_t *buffer)
{
    return RingGetNBytes(xRingBuffer, buffer, xRingBuffer->u8available);
}

/**
 * @brief Get N bytes from ring buffer
 * 
 * @note Must be called only after disabling USART RX interrupt
 */
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
 * ============================================================================ */

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
 * ============================================================================ */

/**
 * @brief Initialize Modbus handler
 * 
 * Creates FreeRTOS task, timers, semaphore, and ring buffer.
 * Must be called before ModbusStart().
 * 
 * @param modH Pointer to modbusHandler_t structure
 * 
 * @note This function will halt on error (while(1) loops).
 * @note Ensure heap is large enough for FreeRTOS objects.
 */
void ModbusInit(modbusHandler_t *modH)
{
    if (numberHandlers < MAX_M_HANDLERS)
    {
        /* Initialize ring buffer */
        RingClear(&modH->xBufferRX);
        
        if (modH->uModbusType == MB_SLAVE)
        {
            /* Create Modbus slave task */
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
            /* Create Modbus master task and queue for telegrams */
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
                while(1); /* Error creating timer, check heap and stack size */
            }
            
            modH->QueueTelegramHandle = osMessageQueueNew(MAX_TELEGRAMS, sizeof(modbus_t), &QueueTelegram_attributes);
            
            if (modH->QueueTelegramHandle == NULL)
            {
                while(1); /* Error creating queue for telegrams, check heap and stack size */
            }
        }
        else
        {
            while(1); /* Error: Modbus type not supported, choose a valid Type */
        }
        
        if (modH->myTaskModbusAHandle == NULL)
        {
            while(1); /* Error creating Modbus task, check heap and stack size */
        }
        
        modH->xTimerT35 = xTimerCreate("TimerT35",
                                        T35,
                                        pdFALSE,
                                        (void *)modH->xTimerT35,
                                        (TimerCallbackFunction_t)vTimerCallbackT35);
        
        if (modH->xTimerT35 == NULL)
        {
            while(1); /* Error creating the timer, check heap and stack size */
        }
        
        modH->ModBusSphrHandle = osSemaphoreNew(1, 1, &ModBusSphr_attributes);
        
        if (modH->ModBusSphrHandle == NULL)
        {
            while(1); /* Error creating the semaphore, check heap and stack size */
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
 * ============================================================================ */

/**
 * @brief Start Modbus communication
 * 
 * Call this AFTER calling begin() on the serial port.
 * Enables UART interrupts and initializes state machine.
 * 
 * @param modH Pointer to modbusHandler_t structure
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
        while(1); /* Error: To use USART_HW_DMA enable it in ModbusConfig.h */
    }
    
    if (modH->xTypeHW == USART_HW || modH->xTypeHW == USART_HW_DMA)
    {
        if (modH->EN_Port != NULL)
        {
            /* Return RS485 transceiver to receive mode */
            HAL_GPIO_WritePin(modH->EN_Port, modH->EN_Pin, GPIO_PIN_RESET);
        }
        
        if (modH->uModbusType == MB_SLAVE && modH->u16regs == NULL)
        {
            while(1); /* Error: define the DATA pointer shared through Modbus */
        }
        
        /* Check that port is initialized */
        while (HAL_UART_GetState(modH->port) != HAL_UART_STATE_READY)
        {
            /* Wait for UART ready */
        }
        
#if ENABLE_USART_DMA == 1
        if (modH->xTypeHW == USART_HW_DMA)
        {
            if (HAL_UARTEx_ReceiveToIdle_DMA(modH->port, modH->xBufferRX.uxBuffer, MAX_BUFFER) != HAL_OK)
            {
                while(1); /* Error in initialization code */
            }
            __HAL_DMA_DISABLE_IT(modH->port->hdmarx, DMA_IT_HT); /* Disable half-transfer interrupt */
        }
        else
        {
            /* Receive data from serial port using interrupt */
            if (HAL_UART_Receive_IT(modH->port, &modH->dataRX, 1) != HAL_OK)
            {
                while(1); /* Error in initialization code */
            }
        }
#else
        /* Receive data from serial port using interrupt */
        if (HAL_UART_Receive_IT(modH->port, &modH->dataRX, 1) != HAL_OK)
        {
            while(1); /* Error in initialization code */
        }
#endif
        
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
        while(1); /* Error: define the DATA pointer shared through Modbus */
    }
    
    modH->u8lastRec = modH->u8BufferSize = 0;
    modH->u16InCnt = modH->u16OutCnt = modH->u16errCnt = 0;
}
#endif

/* ============================================================================
 * TIMER CALLBACKS
 * ============================================================================ */

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
 * ============================================================================ */

#if ENABLE_TCP == 1
bool TCPwaitConnData(modbusHandler_t *modH)
{
    struct netbuf inbuf;
    err_t recv_err, accept_err;
    char buf;
    uint16_t buflen;
    uint16_t uLength;
    bool xTCPvalid = false;
    
    tcpclients_t *clientconn;
    
    /* Round-robin connection slot selection */
    modH->newconnIndex++;
    if (modH->newconnIndex >= NUMBERTCPCONN)
    {
        modH->newconnIndex = 0;
    }
    
    clientconn = &modH->newconns[modH->newconnIndex];
    
    if (clientconn->conn == NULL)
    {
        accept_err = netconn_accept(modH->conn, &clientconn->conn);
        if (accept_err != ERR_OK)
        {
            ModbusCloseConnNull(modH);
            return xTCPvalid;
        }
        else
        {
            clientconn->aging = 0;
        }
    }
    
    netconn_set_recvtimeout(clientconn->conn, modH->u16timeOut);
    recv_err = netconn_recv(clientconn->conn, &inbuf);
    
    if (recv_err == ERR_CLSD)
    {
        ModbusCloseConnNull(modH);
        clientconn->aging = 0;
        return xTCPvalid;
    }
    
    if (recv_err == ERR_TIMEOUT)
    {
        modH->newconns[modH->newconnIndex].aging++;
        if (modH->newconns[modH->newconnIndex].aging >= TCPAGINGCYCLES)
        {
            ModbusCloseConnNull(modH);
            clientconn->aging = 0;
        }
        return xTCPvalid;
    }
    
    if (recv_err == ERR_OK)
    {
        if (netconn_err(clientconn->conn) == ERR_OK)
        {
            netbuf_data(inbuf, (void *)&buf, &buflen);
            
            if (buflen > 11) /* Minimum frame size for Modbus TCP */
            {
                if (buf[2] == 0 && buf[3] == 0) /* Validate protocol ID */
                {
                    uLength = (buf[4] << 8 & 0xff00) | buf[5];
                    
                    if (uLength < (MAX_BUFFER - 2) && (uLength + 6) <= buflen)
                    {
                        for (int i = 0; i < uLength; i++)
                        {
                            modH->u8Buffer[i] = buf[i + 6];
                        }
                        modH->u16TransactionID = (buf[0] << 8 & 0xff00) | buf[1];
                        modH->u8BufferSize = uLength + 2; /* Add 2 dummy bytes for CRC */
                        xTCPvalid = true;
                    }
                }
            }
            netbuf_delete(inbuf);
            clientconn->aging = 0;
        }
    }
    
    return xTCPvalid;
}

void TCPinitserver(modbusHandler_t *modH)
{
    err_t err;
    
    if (modH->xTypeHW == TCP_HW)
    {
        modH->conn = netconn_new(NETCONN_TCP);
        
        if (modH->conn != NULL)
        {
            if (modH->uTcpPort == 0) modH->uTcpPort = 502;
            
            err = netconn_bind(modH->conn, NULL, modH->uTcpPort);
            
            if (err == ERR_OK)
            {
                netconn_listen(modH->conn);
                netconn_set_recvtimeout(modH->conn, 1); /* Non-blocking */
            }
            else
            {
                while(1); /* Error binding TCP Modbus port */
            }
        }
        else
        {
            while(1); /* Error creating new connection */
        }
    }
}
#endif

/* ============================================================================
 * MODBUS SLAVE TASK
 * ============================================================================ */

void StartTaskModbusSlave(void *argument)
{
    modbusHandler_t *modH = (modbusHandler_t *)argument;
    
#if ENABLE_TCP == 1
    if (modH->xTypeHW == TCP_HW)
    {
        TCPinitserver(modH); /* Start Modbus TCP server */
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
                continue; /* TCP package was not validated */
            }
        }
#endif
        
        if (modH->xTypeHW == USART_HW || modH->xTypeHW == USART_HW_DMA)
        {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY); /* Block until frame arrives */
            
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
            continue; /* Not for us */
#else
            if (modH->xTypeHW != TCP_HW)
            {
                continue; /* For Modbus TCP this is not validated */
            }
#endif
        }
        
        /* Validate message: CRC, FCT, address and size */
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
                    break; /* Broadcast ignores read functions */
                }
                modH->i8state = process_FC1(modH);
                break;
                
            case MB_FC_READ_INPUT_REGISTER:
            case MB_FC_READ_REGISTERS:
                if (modH->u8AddressMode == ADDRESS_BROADCAST)
                {
                    break; /* Broadcast ignores read functions */
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
 * ============================================================================ */

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
 * TCP CONNECTION MANAGEMENT
 * ============================================================================ */

#if ENABLE_TCP == 1
void ModbusCloseConn(struct netconn *conn)
{
    if (conn != NULL)
    {
        netconn_close(conn);
        netconn_delete(conn);
    }
}

void ModbusCloseConnNull(modbusHandler_t *modH)
{
    if (modH->newconns[modH->newconnIndex].conn != NULL)
    {
        netconn_close(modH->newconns[modH->newconnIndex].conn);
        netconn_delete(modH->newconns[modH->newconnIndex].conn);
        modH->newconns[modH->newconnIndex].conn = NULL;
    }
}
#endif

/* ============================================================================
 * MASTER QUERY TRANSMISSION
 * ============================================================================ */

/**
 * @brief Generate and send query to slave
 * 
 * @param modH Modbus handler
 * @param telegram Modbus telegram structure
 * @return 0 on success, error code otherwise
 */
int8_t SendQuery(modbusHandler_t *modH, modbus_t telegram)
{
    uint8_t u8regsno, u8bytesno;
    uint8_t error = 0;
    
    xSemaphoreTake(modH->ModBusSphrHandle, portMAX_DELAY);
    
    if (modH->u8id != 0) error = ERR_NOT_MASTER;
    if (modH->i8state != COM_IDLE) error = ERR_POLLING;
    if ((telegram.u8id == 0) || (telegram.u8id > 247)) error = ERR_BAD_SLAVE_ID;
    
    if (error)
    {
        modH->i8lastError = error;
        xSemaphoreGive(modH->ModBusSphrHandle);
        return error;
    }
    
    modH->u16regs = telegram.u16reg;
    
    /* Telegram header */
    modH->u8Buffer[ID] = telegram.u8id;
    modH->u8Buffer[FUNC] = telegram.u8fct;
    modH->u8Buffer[ADD_HI] = highByte(telegram.u16RegAdd);
    modH->u8Buffer[ADD_LO] = lowByte(telegram.u16RegAdd);
    
    switch (telegram.u8fct)
    {
        case MB_FC_READ_COILS:
        case MB_FC_READ_DISCRETE_INPUT:
        case MB_FC_READ_REGISTERS:
        case MB_FC_READ_INPUT_REGISTER:
            modH->u8Buffer[NB_HI] = highByte(telegram.u16CoilsNo);
            modH->u8Buffer[NB_LO] = lowByte(telegram.u16CoilsNo);
            modH->u8BufferSize = 6;
            break;
            
        case MB_FC_WRITE_COIL:
            modH->u8Buffer[NB_HI] = ((telegram.u16reg[0] > 0) ? 0xff : 0);
            modH->u8Buffer[NB_LO] = 0;
            modH->u8BufferSize = 6;
            break;
            
        case MB_FC_WRITE_REGISTER:
            modH->u8Buffer[NB_HI] = highByte(telegram.u16reg[0]);
            modH->u8Buffer[NB_LO] = lowByte(telegram.u16reg[0]);
            modH->u8BufferSize = 6;
            break;
            
        case MB_FC_WRITE_MULTIPLE_COILS:
            u8regsno = telegram.u16CoilsNo / 16;
            u8bytesno = u8regsno * 2;
            if ((telegram.u16CoilsNo % 16) != 0)
            {
                u8bytesno++;
                u8regsno++;
            }
            modH->u8Buffer[NB_HI] = highByte(telegram.u16CoilsNo);
            modH->u8Buffer[NB_LO] = lowByte(telegram.u16CoilsNo);
            modH->u8Buffer[BYTE_CNT] = u8bytesno;
            modH->u8BufferSize = 7;
            
            for (uint16_t i = 0; i < u8bytesno; i++)
            {
                if (i % 2)
                {
                    modH->u8Buffer[modH->u8BufferSize] = lowByte(telegram.u16reg[i / 2]);
                }
                else
                {
                    modH->u8Buffer[modH->u8BufferSize] = highByte(telegram.u16reg[i / 2]);
                }
                modH->u8BufferSize++;
            }
            break;
            
        case MB_FC_WRITE_MULTIPLE_REGISTERS:
            modH->u8Buffer[NB_HI] = highByte(telegram.u16CoilsNo);
            modH->u8Buffer[NB_LO] = lowByte(telegram.u16CoilsNo);
            modH->u8Buffer[BYTE_CNT] = (uint8_t)(telegram.u16CoilsNo * 2);
            modH->u8BufferSize = 7;
            
            for (uint16_t i = 0; i < telegram.u16CoilsNo; i++)
            {
                modH->u8Buffer[modH->u8BufferSize] = highByte(telegram.u16reg[i]);
                modH->u8BufferSize++;
                modH->u8Buffer[modH->u8BufferSize] = lowByte(telegram.u16reg[i]);
                modH->u8BufferSize++;
            }
            break;
    }
    
    sendTxBuffer(modH);
    xSemaphoreGive(modH->ModBusSphrHandle);
    
    modH->i8state = COM_WAITING;
    modH->i8lastError = 0;
    
    return 0;
}

/* ============================================================================
 * TCP MASTER FUNCTIONS
 * ============================================================================ */

#if ENABLE_TCP == 1
static mb_err_op_t TCPconnectserver(modbusHandler_t *modH, modbus_t *telegram)
{
    err_t err;
    tcpclients_t *clientconn;
    
    clientconn = &modH->newconns[modH->newconnIndex];
    
    if (telegram->u8clientID >= NUMBERTCPCONN)
    {
        return ERR_BAD_TCP_ID;
    }
    
    if (clientconn->conn == NULL)
    {
        clientconn->conn = netconn_new(NETCONN_TCP);
        
        if (clientconn->conn == NULL)
        {
            while(1); /* Error creating connection */
        }
        
        err = netconn_connect(clientconn->conn, (ip_addr_t *)&telegram->xIpAddress, telegram->u16Port);
        
        if (err != ERR_OK)
        {
            ModbusCloseConnNull(modH);
            return ERR_TIME_OUT;
        }
    }
    
    return ERR_OK;
}

static mb_err_op_t TCPgetRxBuffer(modbusHandler_t *modH)
{
    struct netbuf *inbuf;
    err_t err = ERR_TIME_OUT;
    char *buf;
    uint16_t buflen;
    uint16_t uLength;
    
    tcpclients_t *clientconn;
    clientconn = &modH->newconns[modH->newconnIndex];
    
    netconn_set_recvtimeout(clientconn->conn, modH->u16timeOut);
    err = netconn_recv(clientconn->conn, &inbuf);
    
    uLength = 0;
    
    if (err == ERR_OK)
    {
        err = netconn_err(clientconn->conn);
        
        if (err == ERR_OK)
        {
            err = netbuf_data(inbuf, (void **)&buf, &buflen);
            
            if (err == ERR_OK)
            {
                if ((buflen > 11 && modH->uModbusType == MB_SLAVE) ||
                    (buflen >= 10 && modH->uModbusType == MB_MASTER))
                {
                    if (buf[2] == 0 && buf[3] == 0)
                    {
                        uLength = (buf[4] << 8 & 0xff00) | buf[5];
                        
                        if (uLength < (MAX_BUFFER - 2) && (uLength + 6) <= buflen)
                        {
                            for (int i = 0; i < uLength; i++)
                            {
                                modH->u8Buffer[i] = buf[i + 6];
                            }
                            modH->u16TransactionID = (buf[0] << 8 & 0xff00) | buf[1];
                            modH->u8BufferSize = uLength + 2;
                        }
                    }
                }
            }
            netbuf_delete(inbuf);
        }
    }
    
    return err;
}
#endif

/* ============================================================================
 * MODBUS MASTER TASK
 * ============================================================================ */

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
            modH->newconnIndex = telegram.u8clientID;
            ulNotificationValue = TCPconnectserver(modH, &telegram);
            
            if (ulNotificationValue == ERR_OK)
            {
                SendQuery(modH, telegram);
                ulNotificationValue = TCPgetRxBuffer(modH);
                
                if (ulNotificationValue != ERR_OK)
                {
                    ModbusCloseConnNull(modH);
                }
            }
            else
            {
                ModbusCloseConnNull(modH);
            }
        }
        else
        {
            SendQuery(modH, telegram);
            ulNotificationValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        }
#else
        /* Wait period of silence between frames */
        if (modH->port->Init.BaudRate <= 19200)
            osDelay((int)(35000 / modH->port->Init.BaudRate) + 2);
        else
            osDelay(3);
            
        SendQuery(modH, telegram);
        ulNotificationValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
#endif
        
        modH->i8lastError = 0;
        
        if (ulNotificationValue)
        {
            modH->i8state = COM_IDLE;
            modH->i8lastError = ERR_TIME_OUT;
            modH->u16errCnt++;
            xTaskNotify((TaskHandle_t)telegram.u32CurrentTask, modH->i8lastError, eSetValueWithOverwrite);
            continue;
        }
        
#if ENABLE_USB_CDC == 1 || ENABLE_TCP == 1
        if (modH->xTypeHW == USART_HW)
        {
            getRxBuffer(modH);
        }
#else
        getRxBuffer(modH);
#endif
        
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
                
            case MB_FC_WRITE_COIL:
            case MB_FC_WRITE_REGISTER:
            case MB_FC_WRITE_MULTIPLE_COILS:
            case MB_FC_WRITE_MULTIPLE_REGISTERS:
                /* Nothing to do */
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
 * ============================================================================ */

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
 * ============================================================================ */

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
        HAL_UART_AbortReceive_IT(modH->port);
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
        HAL_UART_Receive_IT(modH->port, &modH->dataRX, 1);
    }
    
    return i16result;
}

uint8_t validateRequest(modbusHandler_t *modH)
{
#if ENABLE_TCP == 1
    uint16_t u16MsgCRC = ((modH->u8Buffer[modH->u8BufferSize - 2] << 8) | 
                           modH->u8Buffer[modH->u8BufferSize - 1]);
    
    if (modH->xTypeHW != TCP_HW)
    {
        if (calcCRC(modH->u8Buffer, modH->u8BufferSize - 2) != u16MsgCRC)
        {
            modH->u16errCnt++;
            return ERR_BAD_CRC;
        }
    }
#else
    uint16_t u16MsgCRC = ((modH->u8Buffer[modH->u8BufferSize - 2] << 8) | 
                           modH->u8Buffer[modH->u8BufferSize - 1]);
    
    if (calcCRC(modH->u8Buffer, modH->u8BufferSize - 2) != u16MsgCRC)
    {
        modH->u16errCnt++;
        return ERR_BAD_CRC;
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
 * ============================================================================ */

uint16_t word(uint8_t H, uint8_t L)
{
    bytesFields W;
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
 * ============================================================================ */

#if ENABLE_USB_CDC == 1
extern uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len);
#endif

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
            HAL_HalfDuplex_EnableTransmitter(modH->port);
            HAL_GPIO_WritePin(modH->EN_Port, modH->EN_Pin, GPIO_PIN_SET);
        }
        
#if ENABLE_USART_DMA == 1
        if (modH->xTypeHW == USART_HW)
        {
#endif
            HAL_UART_Transmit_IT(modH->port, modH->u8Buffer, modH->u8BufferSize);
#if ENABLE_USART_DMA == 1
        }
        else
        {
            HAL_UART_Transmit_DMA(modH->port, modH->u8Buffer, modH->u8BufferSize);
        }
#endif
        
        ulTaskNotifyTake(pdTRUE, 250);
        
#if defined(STM32H7) || defined(STM32F3) || defined(STM32L4) || defined(STM32L082xx) || \
    defined(STM32F7) || defined(STM32WB) || defined(STM32G070xx) || defined(STM32F0) || \
    defined(STM32G431xx) || defined(STM32H5) || defined(STM32G474xx)
        while ((modH->port->Instance->ISR & USART_ISR_TC) == 0)
#else
        while ((modH->port->Instance->SR & USART_SR_TC) == 0)
#endif
        {
            /* Wait until last byte is sent */
        }
        
        if (modH->EN_Port != NULL)
        {
            HAL_GPIO_WritePin(modH->EN_Port, modH->EN_Pin, GPIO_PIN_RESET);
            HAL_HalfDuplex_EnableReceiver(modH->port);
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
        CDC_Transmit_FS(modH->u8Buffer, modH->u8BufferSize);
        
        if (modH->uModbusType == MB_MASTER)
        {
            xTimerReset(modH->xTimerTimeout, 0);
        }
    }
#endif
#if ENABLE_TCP == 1
    else if (modH->xTypeHW == TCP_HW)
    {
        struct netvector xNetVectors[2];
        uint8_t u8MBAPheader[6];
        size_t uBytesWritten;
        
        u8MBAPheader[0] = highByte(modH->u16TransactionID);
        u8MBAPheader[1] = lowByte(modH->u16TransactionID);
        u8MBAPheader[2] = 0;
        u8MBAPheader[3] = 0;
        u8MBAPheader[4] = 0;
        u8MBAPheader[5] = modH->u8BufferSize;
        
        xNetVectors[0].len = 6;
        xNetVectors[0].ptr = (void *)u8MBAPheader;
        
        xNetVectors[1].len = modH->u8BufferSize;
        xNetVectors[1].ptr = (void *)modH->u8Buffer;
        
        netconn_set_sendtimeout(modH->newconns[modH->newconnIndex].conn, modH->u16timeOut);
        
        err_enum_t err = netconn_write_vectors_partly(modH->newconns[modH->newconnIndex].conn, 
                                                       xNetVectors, 2, NETCONN_COPY, &uBytesWritten);
        
        if (err != ERR_OK)
        {
            ModbusCloseConnNull(modH);
        }
        
        if (modH->uModbusType == MB_MASTER)
        {
            xTimerReset(modH->xTimerTimeout, 0);
        }
    }
#endif
#endif
    
    modH->u8BufferSize = 0;
    modH->u16OutCnt++;
}

/* ============================================================================
 * PROCESS FUNCTIONS (SLAVE)
 * ============================================================================ */

/**
 * @brief Process functions 1 & 2 (Read Coils/Discrete Inputs)
 * 
 * @param modH Modbus handler
 * @return Response buffer size
 */
int8_t process_FC1(modbusHandler_t *modH)
{
    uint16_t u16currentRegister;
    uint8_t u8currentBit, u8bytesno, u8bitsno;
    uint8_t u8CopyBufferSize;
    uint16_t u16currentCoil, u16coil;
    
    uint16_t u16StartCoil = word(modH->u8Buffer[ADD_HI], modH->u8Buffer[ADD_LO]);
    uint16_t u16Coilno = word(modH->u8Buffer[NB_HI], modH->u8Buffer[NB_LO]);
    
    u8bytesno = (uint8_t)(u16Coilno / 8);
    if (u16Coilno % 8 != 0) u8bytesno++;
    
    modH->u8Buffer[ADD_HI] = u8bytesno;
    modH->u8BufferSize = ADD_LO;
    modH->u8Buffer[modH->u8BufferSize + u8bytesno - 1] = 0;
    
    u8bitsno = 0;
    
    for (u16currentCoil = 0; u16currentCoil < u16Coilno; u16currentCoil++)
    {
        u16coil = u16StartCoil + u16currentCoil;
        u16currentRegister = (u16coil / 16);
        u8currentBit = (uint8_t)(u16coil % 16);
        
        bitWrite(modH->u8Buffer[modH->u8BufferSize], u8bitsno,
                 bitRead(modH->u16regs[u16currentRegister], u8currentBit));
        
        u8bitsno++;
        
        if (u8bitsno > 7)
        {
            u8bitsno = 0;
            modH->u8BufferSize++;
        }
    }
    
    if (u16Coilno % 8 != 0) modH->u8BufferSize++;
    
    u8CopyBufferSize = modH->u8BufferSize + 2;
    sendTxBuffer(modH);
    
    return u8CopyBufferSize;
}

/**
 * @brief Process functions 3 & 4 (Read Registers)
 * 
 * KEY CHANGE: Supports flexible data hooks for project-specific serialization.
 * Falls back to u16regs array if hooks are NULL (backward compatibility).
 * 
 * @param modH Modbus handler
 * @return Response buffer size
 */
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
        /* If data layer is enabled and hook is set, use it */
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

/**
 * @brief Process function 5 (Write Single Coil)
 * 
 * @param modH Modbus handler
 * @return Response buffer size
 */
int8_t process_FC5(modbusHandler_t *modH)
{
    uint8_t u8currentBit;
    uint16_t u16currentRegister;
    uint8_t u8CopyBufferSize;
    
    uint16_t u16coil = word(modH->u8Buffer[ADD_HI], modH->u8Buffer[ADD_LO]);
    
    u16currentRegister = (u16coil / 16);
    u8currentBit = (uint8_t)(u16coil % 16);
    
    bitWrite(modH->u16regs[u16currentRegister], u8currentBit,
             modH->u8Buffer[NB_HI] == 0xff);
    
    modH->u8BufferSize = 6;
    u8CopyBufferSize = modH->u8BufferSize + 2;
    sendTxBuffer(modH);
    
    return u8CopyBufferSize;
}

/**
 * @brief Process function 6 (Write Single Register)
 * 
 * KEY CHANGE: Supports flexible data hooks for project-specific write handling.
 * Falls back to u16regs array if hooks are NULL (backward compatibility).
 * 
 * @param modH Modbus handler
 * @return Response buffer size
 */
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

/**
 * @brief Process function 15 (Write Multiple Coils)
 * 
 * @param modH Modbus handler
 * @return Response buffer size
 */
int8_t process_FC15(modbusHandler_t *modH)
{
    uint8_t u8currentBit, u8frameByte, u8bitsno;
    uint16_t u16currentRegister;
    uint8_t u8CopyBufferSize;
    uint16_t u16currentCoil, u16coil;
    bool bTemp;
    
    uint16_t u16StartCoil = word(modH->u8Buffer[ADD_HI], modH->u8Buffer[ADD_LO]);
    uint16_t u16Coilno = word(modH->u8Buffer[NB_HI], modH->u8Buffer[NB_LO]);
    
    u8bitsno = 0;
    u8frameByte = 7;
    
    for (u16currentCoil = 0; u16currentCoil < u16Coilno; u16currentCoil++)
    {
        u16coil = u16StartCoil + u16currentCoil;
        u16currentRegister = (u16coil / 16);
        u8currentBit = (uint8_t)(u16coil % 16);
        
        bTemp = bitRead(modH->u8Buffer[u8frameByte], u8bitsno);
        
        bitWrite(modH->u16regs[u16currentRegister], u8currentBit, bTemp);
        
        u8bitsno++;
        
        if (u8bitsno > 7)
        {
            u8bitsno = 0;
            u8frameByte++;
        }
    }
    
    modH->u8BufferSize = 6;
    u8CopyBufferSize = modH->u8BufferSize + 2;
    sendTxBuffer(modH);
    
    return u8CopyBufferSize;
}

/**
 * @brief Process function 16 (Write Multiple Registers)
 * 
 * KEY CHANGE: Supports flexible data hooks for project-specific write handling.
 * Falls back to u16regs array if hooks are NULL (backward compatibility).
 * 
 * @param modH Modbus handler
 * @return Response buffer size
 */
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