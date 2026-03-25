/*
 * Modbus.h
 * Modbus RTU Master and Slave library for STM32 CUBE with FreeRTOS
 * 
 * ============================================================================
 * INTEGRATION REQUIREMENT (IMPORTANT!)
 * ============================================================================
 * 
 * Project must include main.h BEFORE Modbus.h to provide HAL type definitions.
 * 
 * Example in main.c:
 *   #include "main.h"      <- MUST be first (provides UART_HandleTypeDef, etc.)
 *   #include "Modbus.h"    <- Then Modbus library
 * 
 * Reason: This library uses UART_HandleTypeDef and GPIO_TypeDef from STM32 HAL.
 *         Forward declarations cause type conflicts with real HAL definitions.
 * ============================================================================
 * 
 * Created on: May 5, 2020
 * Author: Alejandro Mera
 * Refactored for flexible handler support
 */

#ifndef THIRD_PARTY_MODBUS_INC_MODBUS_H_
#define THIRD_PARTY_MODBUS_INC_MODBUS_H_

/* ============================================================================
 * INCLUDES
 * ============================================================================
 */
#include "ModbusConfig.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

#include "ports/modbus_port.h"

/* NOTE: Do NOT include main.h here - project must include it before this file */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONFIGURATION CHECKS
 * ============================================================================
 */
#ifndef MAX_BUFFER
#define MAX_BUFFER 256
#endif

#ifndef MAX_M_HANDLERS
#define MAX_M_HANDLERS 4
#endif

#ifndef T35
#define T35 pdMS_TO_TICKS(2)
#endif

/* ============================================================================
 * ENUMERATIONS
 * ============================================================================
 */

/**
 * @brief Hardware type enumeration
 */
typedef enum
{
    USART_HW = 1,           /**< Standard USART interrupt mode */
    USB_CDC_HW = 2,         /**< USB CDC device */
    TCP_HW = 3,             /**< TCP/IP network */
    USART_HW_DMA = 4,       /**< USART with DMA support */
} mb_hardware_t;

/**
 * @brief Master/Slave mode enumeration
 */
typedef enum
{
    MB_SLAVE = 3,           /**< Slave mode */
    MB_MASTER = 4,          /**< Master mode */
} mb_masterslave_t;

/**
 * @brief Address mode enumeration
 */
typedef enum
{
    ADDRESS_BROADCAST = 0,  /**< Broadcast mode (ID = 0) */
    ADDRESS_NORMAL = 1,     /**< Normal mode (ID > 0) */
} mb_address_t;

/**
 * @brief Modbus function codes
 */
typedef enum MB_FC
{
    MB_FC_READ_COILS = 1,               /**< Function code 1: Read coils */
    MB_FC_READ_DISCRETE_INPUT = 2,      /**< Function code 2: Read discrete inputs */
    MB_FC_READ_REGISTERS = 3,           /**< Function code 3: Read holding registers */
    MB_FC_READ_INPUT_REGISTER = 4,      /**< Function code 4: Read input registers */
    MB_FC_WRITE_COIL = 5,               /**< Function code 5: Write single coil */
    MB_FC_WRITE_REGISTER = 6,           /**< Function code 6: Write single register */
    MB_FC_WRITE_MULTIPLE_COILS = 15,    /**< Function code 15: Write multiple coils */
    MB_FC_WRITE_MULTIPLE_REGISTERS = 16 /**< Function code 16: Write multiple registers */
} mb_functioncode_t;

/**
 * @brief Communication states
 */
typedef enum COM_STATES
{
    COM_IDLE = 0,       /**< Idle state, ready for new message */
    COM_WAITING = 1,    /**< Waiting for response (Master only) */
} mb_com_state_t;

/**
 * @brief Error and operation codes
 */
typedef enum ERR_OP_LIST
{
    /* Errors */
    ERR_NOT_MASTER = 10,            /**< Operation not allowed for slave */
    ERR_POLLING = 11,               /**< Polling error */
    ERR_BUFF_OVERFLOW = 12,         /**< Buffer overflow */
    ERR_BAD_CRC = 13,               /**< CRC mismatch */
    ERR_EXCEPTION = 14,             /**< Modbus exception received */
    ERR_BAD_SIZE = 15,              /**< Invalid message size */
    ERR_BAD_ADDRESS = 16,           /**< Invalid address */
    ERR_TIME_OUT = 17,              /**< Timeout error */
    ERR_BAD_SLAVE_ID = 18,          /**< Invalid slave ID */
    ERR_BAD_TCP_ID = 19,            /**< Invalid TCP connection ID */
    /* Operations */
    OP_OK_QUERY = 20                /**< Query completed successfully */
} mb_err_op_t;

/**
 * @brief Exception codes
 */
typedef enum
{
    EXC_FUNC_CODE = 1,      /**< Illegal function code */
    EXC_ADDR_RANGE = 2,     /**< Illegal data address */
    EXC_REGS_QUANT = 3,     /**< Illegal data value */
    EXC_EXECUTE = 4         /**< Slave device failure */
} mb_exception_t;

/**
 * @brief Message frame indexes
 */
typedef enum MESSAGE
{
    ID = 0,         /**< Slave ID field */
    FUNC,           /**< Function code */
    ADD_HI,         /**< Address high byte */
    ADD_LO,         /**< Address low byte */
    NB_HI,          /**< Number of registers/coils high byte */
    NB_LO,          /**< Number of registers/coils low byte */
    BYTE_CNT        /**< Byte count field */
} mb_message_t;

/* ============================================================================
 * TYPE DEFINITIONS
 * ============================================================================
 */

/**
 * @brief Union for byte manipulation
 */
typedef union {
    uint8_t  u8[4];
    uint16_t u16[2];
    uint32_t u32;
} bytesFields_t;

/**
 * @brief Ring buffer structure for USART reception
 */
typedef struct
{
    uint8_t uxBuffer[MAX_BUFFER];   /**< Buffer storage */
    uint8_t u8start;                /**< Start index */
    uint8_t u8end;                  /**< End index */
    uint8_t u8available;            /**< Available bytes count */
    bool    overflow;               /**< Overflow flag */
} modbusRingBuffer_t;

/**
 * @brief Master query structure
 */
typedef struct
{
    uint8_t u8id;                   /**< Slave address (1-247, 0=broadcast) */
    mb_functioncode_t u8fct;        /**< Function code */
    uint16_t u16RegAdd;             /**< Starting register address */
    uint16_t u16CoilsNo;            /**< Number of coils/registers to access */
    uint16_t *u16reg;               /**< Pointer to data buffer */
    uint32_t u32CurrentTask;        /**< Calling task handle for notifications */
#if ENABLE_TCP == 1
    uint32_t xIpAddress;            /**< TCP server IP address */
    uint16_t u16Port;               /**< TCP server port */
    uint8_t u8clientID;             /**< TCP connection slot ID */
#endif
} modbus_t;

/* ============================================================================
 * FORWARD DECLARATIONS FOR FLEXIBLE DATA HANDLING
 * ============================================================================
 */
struct ModbusDataResponse;
typedef struct ModbusDataResponse ModbusDataResponse_t;

struct ModbusWriteRequest;
typedef struct ModbusWriteRequest ModbusWriteRequest_t;

/* ============================================================================
 * APPLICATION HOOK TYPES (FLEXIBLE DATA ACCESS)
 * ============================================================================
 */

/**
 * @brief Read hook function type for flexible register access
 * @param address Register address being read
 * @param response Pointer to response structure to fill
 * @return true if successful, false if error
 */
typedef bool (*ModbusReadFlexHook_t)(uint16_t address, ModbusDataResponse_t *response);

/**
 * @brief Write hook function type for flexible register access
 * @param address Register address being written
 * @param request Pointer to request structure containing data
 * @return true if successful, false if error
 */
typedef bool (*ModbusWriteFlexHook_t)(uint16_t address, const ModbusWriteRequest_t *request);

/* ============================================================================
 * TCP CONNECTION STRUCTURE
 * ============================================================================
 */
#if ENABLE_TCP == 1
typedef struct
{
    struct netconn *conn;       /**< Network connection handle */
    uint32_t aging;             /**< Connection age counter */
} tcpclients_t;

#ifndef NUMBERTCPCONN
#define NUMBERTCPCONN 4
#endif

#ifndef TCPAGINGCYCLES
#define TCPAGINGCYCLES 100
#endif
#endif

/* ============================================================================
 * MODBUS HANDLER STRUCTURE
 * ============================================================================
 */

/**
 * @brief Modbus handler structure
 * Contains all variables required for Modbus daemon operation.
 */
typedef struct
{
    /* ========================================================================
     * BASIC CONFIGURATION
     * ========================================================================
     */
    mb_masterslave_t uModbusType;       /**< Master or Slave mode */
    ModbusUartHandle_t *port;
    uint8_t u8id;                       /**< Slave ID (0=master, 1-247=slave) */
    ModbusGpioPort_t *EN_Port;
    ModbusGpioPin_t EN_Pin;
    mb_hardware_t xTypeHW;              /**< Hardware type */
    
    /* ========================================================================
     * DATA BUFFERS
     * ========================================================================
     */
    uint8_t u8Buffer[MAX_BUFFER];       /**< Modbus communication buffer */
    uint8_t u8BufferSize;               /**< Current buffer size */
    uint8_t u8lastRec;                  /**< Last received byte index */
    uint16_t *u16regs;                  /**< Pointer to register array (legacy) */
    uint16_t u16regsize;                /**< Size of register array */
    uint8_t dataRX;                     /**< Single byte for UART RX */
    
    /* ========================================================================
     * COMMUNICATION STATE
     * ========================================================================
     */
    mb_err_op_t i8lastError;            /**< Last error code */
    int8_t i8state;                     /**< Current communication state */
    mb_address_t u8AddressMode;         /**< Broadcast or normal mode */
    uint16_t u16InCnt;                  /**< Incoming message counter */
    uint16_t u16OutCnt;                 /**< Outgoing message counter */
    uint16_t u16errCnt;                 /**< Error counter */
    uint16_t u16timeOut;                /**< Communication timeout (ms) */
    
    /* ========================================================================
     * TCP SPECIFIC FIELDS
     * ========================================================================
     */
#if ENABLE_TCP == 1
    tcpclients_t newconns[NUMBERTCPCONN];   /**< TCP connection pool */
    struct netconn *conn;                   /**< Server connection handle */
    uint32_t xIpAddress;                    /**< Client IP address */
    uint16_t u16TransactionID;              /**< MBAP transaction ID */
    uint16_t uTcpPort;                      /**< TCP port */
    uint8_t newconnIndex;                   /**< Current connection index */
#endif
    
    /* ========================================================================
     * FREERTOS COMPONENTS
     * ========================================================================
     */
    osMessageQueueId_t QueueTelegramHandle; /**< Queue for master telegrams */
    osThreadId_t myTaskModbusAHandle;       /**< Modbus task handle */
    TimerHandle_t xTimerT35;                /**< T3.5 timing timer */
    TimerHandle_t xTimerTimeout;            /**< Master timeout timer */
    osSemaphoreId_t ModBusSphrHandle;       /**< Data access semaphore */
    modbusRingBuffer_t xBufferRX;           /**< USART RX ring buffer */
    
    /* ========================================================================
     * APPLICATION INTEGRATION HOOKS (FLEXIBLE DATA ACCESS)
     * ========================================================================
     */
    ModbusReadFlexHook_t onReadFlex;        /**< Flexible read hook */
    ModbusWriteFlexHook_t onWriteFlex;      /**< Flexible write hook */
    void *appContext;                       /**< Application context pointer */
    
    /* ========================================================================
     * LEGACY SUPPORT HOOKS (OPTIONAL)
     * ========================================================================
     */
    uint16_t (*onReadSimple)(uint16_t addr);            /**< Simple read hook */
    bool (*onWriteSimple)(uint16_t addr, uint16_t value); /**< Simple write hook */

    /* ========================================================================
     * DYNAMIC HANDLER MODE FLAG
     * ========================================================================
     */
    bool dynamic_handlers;                  /**< Use registry, ignore u16regs */
    
} modbusHandler_t;

/* ============================================================================
 * CONSTANTS
 * ============================================================================
 */
typedef enum
{
    RESPONSE_SIZE = 6,      /**< Normal response frame size */
    EXCEPTION_SIZE = 3,     /**< Exception response frame size */
    CHECKSUM_SIZE = 2       /**< CRC checksum size */
} modbus_constants_t;

/* ============================================================================
 * GLOBAL VARIABLES
 * ============================================================================
 */
extern modbusHandler_t *mHandlers[MAX_M_HANDLERS];
extern uint8_t numberHandlers;

/* ============================================================================
 * PUBLIC API - INITIALIZATION
 * ============================================================================
 */
void ModbusInit(modbusHandler_t *modH);
void ModbusStart(modbusHandler_t *modH);
#if ENABLE_USB_CDC == 1
void ModbusStartCDC(modbusHandler_t *modH);
#endif

/* ============================================================================
 * PUBLIC API - MASTER OPERATIONS
 * ============================================================================
 */
void ModbusQuery(modbusHandler_t *modH, modbus_t telegram);
uint32_t ModbusQueryV2(modbusHandler_t *modH, modbus_t telegram);
void ModbusQueryInject(modbusHandler_t *modH, modbus_t telegram);

/* ============================================================================
 * PUBLIC API - TIMING
 * ============================================================================
 */
void setTimeOut(uint16_t u16timeOut);
uint16_t getTimeOut(void);
bool getTimeOutState(void);

/* ============================================================================
 * PUBLIC API - TASKS
 * ============================================================================
 */
void StartTaskModbusSlave(void *argument);
void StartTaskModbusMaster(void *argument);

/* ============================================================================
 * PUBLIC API - UTILITIES
 * ============================================================================
 */
uint16_t calcCRC(uint8_t *Buffer, uint8_t u8length);

/* ============================================================================
 * PUBLIC API - TCP (IF ENABLED)
 * ============================================================================
 */
#if ENABLE_TCP == 1
void ModbusCloseConn(struct netconn *conn);
void ModbusCloseConnNull(modbusHandler_t *modH);
#endif

/* ============================================================================
 * PUBLIC API - RING BUFFER
 * ============================================================================
 */
void RingAdd(modbusRingBuffer_t *xRingBuffer, uint8_t u8Val);
uint8_t RingGetAllBytes(modbusRingBuffer_t *xRingBuffer, uint8_t *buffer);
uint8_t RingGetNBytes(modbusRingBuffer_t *xRingBuffer, uint8_t *buffer, uint8_t uNumber);
uint8_t RingCountBytes(modbusRingBuffer_t *xRingBuffer);
void RingClear(modbusRingBuffer_t *xRingBuffer);

#ifdef __cplusplus
}
#endif

#endif /* THIRD_PARTY_MODBUS_INC_MODBUS_H_ */