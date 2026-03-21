/*
 * Modbus.h
 * 
 *  Created on: May 5, 2020
 *      Author: Alejandro Mera
 */

#ifndef THIRD_PARTY_MODBUS_INC_MODBUS_H_
#define THIRD_PARTY_MODBUS_INC_MODBUS_H_

/* ============================================================================
 * INCLUDES
 * ============================================================================ */

#include "ModbusConfig.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONFIGURATION CHECKS
 * ============================================================================ */

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
 * ============================================================================ */

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
 * 
 * These are the implemented function codes for both Master and Slave.
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
 * ============================================================================ */

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
 * 
 * This structure contains all fields necessary to generate a Modbus query.
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
 * ============================================================================ */

/**
 * @brief Forward declaration for data response structure
 * 
 * Defined in extensions/modbus_data.h when MODBUS_DATA_LAYER_ENABLED is set.
 * This allows the core library to reference the type without including the header.
 */
struct ModbusDataResponse;
typedef struct ModbusDataResponse ModbusDataResponse_t;

/**
 * @brief Forward declaration for write request structure
 */
struct ModbusWriteRequest;
typedef struct ModbusWriteRequest ModbusWriteRequest_t;

/* ============================================================================
 * APPLICATION HOOK TYPES (FLEXIBLE DATA ACCESS)
 * ============================================================================ */

/**
 * @brief Read hook function type for flexible register access
 * 
 * This hook is called when a read request (FC3/FC4) is received.
 * The handler should fill the response structure with data to be sent.
 * 
 * @param address Register address being read
 * @param response Pointer to response structure to fill
 * @return true if successful, false if address not found or error
 * 
 * @note This enables project-specific data handling without modifying the library.
 * @note The response data must remain valid until after the function returns.
 */
typedef bool (*ModbusReadFlexHook_t)(uint16_t address, ModbusDataResponse_t *response);

/**
 * @brief Write hook function type for flexible register access
 * 
 * This hook is called when a write request (FC6/FC16) is received.
 * The handler should process the incoming data and return success/failure.
 * 
 * @param address Register address being written
 * @param request Pointer to request structure containing data
 * @return true if successful, false if error (will send exception response)
 * 
 * @note This enables commands, validation, and project-specific logic.
 * @note Keep execution time under 1ms to avoid Modbus timeout.
 */
typedef bool (*ModbusWriteFlexHook_t)(uint16_t address, const ModbusWriteRequest_t *request);

/* ============================================================================
 * TCP CONNECTION STRUCTURE
 * ============================================================================ */

#if ENABLE_TCP == 1
/**
 * @brief TCP client connection structure
 */
typedef struct
{
    struct netconn *conn;       /**< Network connection handle */
    uint32_t aging;             /**< Connection age counter for cleanup */
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
 * ============================================================================ */

/**
 * @brief Modbus handler structure
 * 
 * Contains all variables required for Modbus daemon operation.
 * Each instance represents one Modbus channel (UART, TCP, USB-CDC, etc.).
 */
typedef struct
{
    /* ========================================================================
     * BASIC CONFIGURATION
     * ======================================================================== */
    mb_masterslave_t uModbusType;       /**< Master or Slave mode */
    UART_HandleTypeDef *port;           /**< HAL UART handle */
    uint8_t u8id;                       /**< Slave ID (0=master, 1-247=slave) */
    GPIO_TypeDef *EN_Port;              /**< RS485 direction control port */
    uint16_t EN_Pin;                    /**< RS485 direction control pin */
    mb_hardware_t xTypeHW;              /**< Hardware type (USART, TCP, etc.) */
    
    /* ========================================================================
     * DATA BUFFERS
     * ======================================================================== */
    uint8_t u8Buffer[MAX_BUFFER];       /**< Modbus communication buffer */
    uint8_t u8BufferSize;               /**< Current buffer size */
    uint8_t u8lastRec;                  /**< Last received byte index */
    uint16_t *u16regs;                  /**< Pointer to register array (legacy) */
    uint16_t u16regsize;                /**< Size of register array */
    uint8_t dataRX;                     /**< Single byte for UART RX */
    
    /* ========================================================================
     * COMMUNICATION STATE
     * ======================================================================== */
    mb_err_op_t i8lastError;            /**< Last error code */
    int8_t i8state;                     /**< Current communication state */
    mb_address_t u8AddressMode;         /**< Broadcast or normal mode */
    uint16_t u16InCnt;                  /**< Incoming message counter */
    uint16_t u16OutCnt;                 /**< Outgoing message counter */
    uint16_t u16errCnt;                 /**< Error counter */
    uint16_t u16timeOut;                /**< Communication timeout (ms) */
    
    /* ========================================================================
     * TCP SPECIFIC FIELDS
     * ======================================================================== */
#if ENABLE_TCP == 1
    tcpclients_t newconns[NUMBERTCPCONN];   /**< TCP connection pool */
    struct netconn *conn;                   /**< Server connection handle */
    uint32_t xIpAddress;                    /**< Client IP address */
    uint16_t u16TransactionID;              /**< MBAP transaction ID */
    uint16_t uTcpPort;                      /**< TCP port (default 502) */
    uint8_t newconnIndex;                   /**< Current connection index */
#endif
    
    /* ========================================================================
     * FREERTOS COMPONENTS
     * ======================================================================== */
    osMessageQueueId_t QueueTelegramHandle; /**< Queue for master telegrams */
    osThreadId_t myTaskModbusAHandle;       /**< Modbus task handle */
    TimerHandle_t xTimerT35;                /**< T3.5 timing timer */
    TimerHandle_t xTimerTimeout;            /**< Master timeout timer */
    osSemaphoreId_t ModBusSphrHandle;       /**< Data access semaphore */
    modbusRingBuffer_t xBufferRX;           /**< USART RX ring buffer */
    
    /* ========================================================================
     * APPLICATION INTEGRATION HOOKS (NEW - FLEXIBLE DATA ACCESS)
     * ======================================================================== */
    
    /**
     * @brief Flexible read hook
     * 
     * Called for FC3/FC4 read requests. If NULL, falls back to u16regs array.
     * Enables project-specific data handling without modifying library core.
     */
    ModbusReadFlexHook_t onReadFlex;
    
    /**
     * @brief Flexible write hook
     * 
     * Called for FC6/FC16 write requests. If NULL, falls back to u16regs array.
     * Enables commands, validation, and custom logic.
     */
    ModbusWriteFlexHook_t onWriteFlex;
    
    /**
     * @brief Application context pointer
     * 
     * Opaque pointer passed to hooks for project-specific data.
     * Can be used to access configuration, state, or other context.
     */
    void *appContext;
    
    /* ========================================================================
     * LEGACY SUPPORT HOOKS (OPTIONAL - BACKWARD COMPATIBILITY)
     * ======================================================================== */
    
    /**
     * @brief Simple read hook (16-bit only)
     * 
     * Legacy interface for simple register reads.
     * Returns 16-bit value directly.
     */
    uint16_t (*onReadSimple)(uint16_t addr);
    
    /**
     * @brief Simple write hook (16-bit only)
     * 
     * Legacy interface for simple register writes.
     * Returns success/failure status.
     */
    bool (*onWriteSimple)(uint16_t addr, uint16_t value);
    
} modbusHandler_t;

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/**
 * @brief Response frame sizes
 */
typedef enum
{
    RESPONSE_SIZE = 6,      /**< Normal response frame size */
    EXCEPTION_SIZE = 3,     /**< Exception response frame size */
    CHECKSUM_SIZE = 2       /**< CRC checksum size */
} modbus_constants_t;

/* ============================================================================
 * GLOBAL VARIABLES
 * ============================================================================ */

/**
 * @brief Array of active Modbus handlers
 * 
 * Supports multiple concurrent Modbus instances (limited by MAX_M_HANDLERS).
 */
extern modbusHandler_t *mHandlers[MAX_M_HANDLERS];

/**
 * @brief Number of registered handlers
 */
extern uint8_t numberHandlers;

/* ============================================================================
 * PUBLIC API - INITIALIZATION
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
void ModbusInit(modbusHandler_t *modH);

/**
 * @brief Start Modbus communication
 * 
 * Enables UART interrupts and initializes state machine.
 * Must be called after ModbusInit() and after UART is configured.
 * 
 * @param modH Pointer to modbusHandler_t structure
 */
void ModbusStart(modbusHandler_t *modH);

#if ENABLE_USB_CDC == 1
/**
 * @brief Start Modbus over USB-CDC
 * 
 * Alternative start function for USB-CDC hardware.
 * 
 * @param modH Pointer to modbusHandler_t structure
 */
void ModbusStartCDC(modbusHandler_t *modH);
#endif

/* ============================================================================
 * PUBLIC API - MASTER OPERATIONS
 * ============================================================================ */

/**
 * @brief Send query to queue (non-blocking)
 * 
 * Adds telegram to master queue for asynchronous transmission.
 * 
 * @param modH Pointer to modbusHandler_t structure
 * @param telegram Modbus telegram structure
 */
void ModbusQuery(modbusHandler_t *modH, modbus_t telegram);

/**
 * @brief Send query and wait for response (blocking)
 * 
 * Adds telegram to queue and waits for notification.
 * Returns operation result via task notification.
 * 
 * @param modH Pointer to modbusHandler_t structure
 * @param telegram Modbus telegram structure
 * @return Operation result (OP_OK_QUERY or error code)
 */
uint32_t ModbusQueryV2(modbusHandler_t *modH, modbus_t telegram);

/**
 * @brief Inject query at queue head (high priority)
 * 
 * Places telegram at front of queue for immediate transmission.
 * 
 * @param modH Pointer to modbusHandler_t structure
 * @param telegram Modbus telegram structure
 */
void ModbusQueryInject(modbusHandler_t *modH, modbus_t telegram);

/* ============================================================================
 * PUBLIC API - TIMING
 * ============================================================================ */

/**
 * @brief Set communication timeout
 * 
 * @param u16timeOut Timeout value in milliseconds
 */
void setTimeOut(uint16_t u16timeOut);

/**
 * @brief Get current timeout value
 * 
 * @return Timeout value in milliseconds
 */
uint16_t getTimeOut(void);

/**
 * @brief Get timeout timer state
 * 
 * @return true if timer is running, false otherwise
 */
bool getTimeOutState(void);

/* ============================================================================
 * PUBLIC API - TASKS
 * ============================================================================ */

/**
 * @brief Modbus slave task entry point
 * 
 * Main loop for slave operation. Created by ModbusInit().
 * 
 * @param argument Pointer to modbusHandler_t structure
 */
void StartTaskModbusSlave(void *argument);

/**
 * @brief Modbus master task entry point
 * 
 * Main loop for master operation. Created by ModbusInit().
 * 
 * @param argument Pointer to modbusHandler_t structure
 */
void StartTaskModbusMaster(void *argument);

/* ============================================================================
 * PUBLIC API - UTILITIES
 * ============================================================================ */

/**
 * @brief Calculate CRC-16 for Modbus
 * 
 * @param Buffer Pointer to data buffer
 * @param u8length Number of bytes to process
 * @return Calculated CRC value
 */
uint16_t calcCRC(uint8_t *Buffer, uint8_t u8length);

/* ============================================================================
 * PUBLIC API - TCP (IF ENABLED)
 * ============================================================================ */

#if ENABLE_TCP == 1
/**
 * @brief Close TCP connection
 * 
 * @param conn Network connection handle
 */
void ModbusCloseConn(struct netconn *conn);

/**
 * @brief Close TCP connection and clean handler
 * 
 * @param modH Pointer to modbusHandler_t structure
 */
void ModbusCloseConnNull(modbusHandler_t *modH);
#endif

/* ============================================================================
 * PUBLIC API - RING BUFFER
 * ============================================================================ */

/**
 * @brief Add byte to ring buffer
 * 
 * @param xRingBuffer Pointer to ring buffer structure
 * @param u8Val Byte value to add
 */
void RingAdd(modbusRingBuffer_t *xRingBuffer, uint8_t u8Val);

/**
 * @brief Get all available bytes from ring buffer
 * 
 * @param xRingBuffer Pointer to ring buffer structure
 * @param buffer Output buffer
 * @return Number of bytes read
 */
uint8_t RingGetAllBytes(modbusRingBuffer_t *xRingBuffer, uint8_t *buffer);

/**
 * @brief Get N bytes from ring buffer
 * 
 * @param xRingBuffer Pointer to ring buffer structure
 * @param buffer Output buffer
 * @param uNumber Number of bytes to read
 * @return Number of bytes actually read
 */
uint8_t RingGetNBytes(modbusRingBuffer_t *xRingBuffer, uint8_t *buffer, uint8_t uNumber);

/**
 * @brief Get count of available bytes
 * 
 * @param xRingBuffer Pointer to ring buffer structure
 * @return Number of available bytes
 */
uint8_t RingCountBytes(modbusRingBuffer_t *xRingBuffer);

/**
 * @brief Clear ring buffer
 * 
 * @param xRingBuffer Pointer to ring buffer structure
 */
void RingClear(modbusRingBuffer_t *xRingBuffer);

/* ============================================================================
 * DEPRECATED/UNIMPLEMENTED FUNCTIONS
 * ============================================================================ */

/*
 * The following functions are declared in the original library but not implemented:
 * 
 * uint16_t getInCnt();          // Number of incoming messages
 * uint16_t getOutCnt();         // Number of outcoming messages
 * uint16_t getErrCnt();         // Error counter
 * uint8_t getID();              // Get slave ID
 * uint8_t getState();           // Get communication state
 * uint8_t getLastError();       // Get last error code
 * void setID(uint8_t u8id);     // Set slave ID
 * void setTxendPinOverTime(uint32_t u32overTime);
 * void ModbusEnd();             // Release serial port
 */

#ifdef __cplusplus
}
#endif

#endif /* THIRD_PARTY_MODBUS_INC_MODBUS_H_ */