/*
 * modbus_data.h
 * 
 * Modbus Data Layer - Generic serialization/deserialization for flexible register access
 * 
 * This module provides type-safe data serialization for Modbus register operations.
 * It supports multiple data types (U16, U32, PACKED, RAW, STRUCT) without knowing
 * about project-specific structures.
 * 
 */

#ifndef MODBUS_DATA_H
#define MODBUS_DATA_H

/* ============================================================================
 * INCLUDES
 * ============================================================================ */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONFIGURATION
 * ============================================================================ */

/**
 * @brief Maximum payload size for Modbus responses
 * 
 * Modbus RTU limits: 253 bytes max payload (256 - 3 header bytes)
 * Adjust based on your MAX_BUFFER setting in ModbusConfig.h
 */
#ifndef MODBUS_DATA_MAX_BYTES
#define MODBUS_DATA_MAX_BYTES   250
#endif

/* ============================================================================
 * DATA TYPES ENUMERATION
 * ============================================================================ */

/**
 * @brief Supported data types for Modbus register access
 * 
 * Each type defines how data is serialized/deserialized:
 * - U16: Single 16-bit register (2 bytes, big-endian)
 * - U32: Two 16-bit registers (4 bytes, big-endian)
 * - I32: Two 16-bit registers (4 bytes, big-endian, signed)
 * - PACKED: Arbitrary byte array (for unions with mixed types)
 * - RAW: Raw byte array (for strings, blobs)
 * - STRUCT: Array of uint16_t registers
 */
typedef enum
{
    MODBUS_TYPE_U16 = 0,        /**< 1 register, 2 bytes, big-endian */
    MODBUS_TYPE_U32 = 1,        /**< 2 registers, 4 bytes, big-endian */
    MODBUS_TYPE_I32 = 2,        /**< 2 registers, 4 bytes, big-endian, signed */
    MODBUS_TYPE_PACKED = 3,     /**< Arbitrary bytes (for unions) */
    MODBUS_TYPE_RAW = 4,        /**< Raw bytes (strings, blobs) */
    MODBUS_TYPE_STRUCT = 5,     /**< Array of uint16_t registers */
} ModbusDataType_t;

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

/**
 * @brief Response structure for read operations
 * 
 * This structure is filled by application read hooks to specify
 * what data should be sent in the Modbus response.
 * 
 * @note The 'value' pointer must remain valid until after serialization completes.
 *       Use static variables or ensure data lifetime exceeds function scope.
 */
typedef struct ModbusDataResponse
{
    ModbusDataType_t type;      /**< Data type for serialization */
    const void *value;          /**< Pointer to data buffer */
    uint16_t byte_count;        /**< Actual number of bytes to send */
    uint16_t reg_count;         /**< Number of registers this occupies */
    bool is_valid;              /**< Flag: data is valid and ready */
} ModbusDataResponse_t;

/**
 * @brief Request structure for write operations
 * 
 * This structure contains incoming data from Modbus write requests.
 * Application write hooks parse this data according to their expected format.
 */
typedef struct ModbusWriteRequest
{
    uint16_t address;           /**< Register address being written */
    const uint8_t *data;        /**< Pointer to raw bytes from Modbus frame */
    uint16_t byte_count;        /**< Number of bytes in the request */
    uint16_t reg_count;         /**< Number of registers requested */
} ModbusWriteRequest_t;

/* ============================================================================
 * HELPER MACROS
 * ============================================================================ */

/**
 * @brief Create U16 response (single register)
 * @param ptr Pointer to uint16_t value
 */
#define MODBUS_RESP_U16(ptr) \
    { .type = MODBUS_TYPE_U16, .value = (ptr), .byte_count = 2, .reg_count = 1, .is_valid = true }

/**
 * @brief Create U32 response (two registers)
 * @param ptr Pointer to uint32_t value
 */
#define MODBUS_RESP_U32(ptr) \
    { .type = MODBUS_TYPE_U32, .value = (ptr), .byte_count = 4, .reg_count = 2, .is_valid = true }

/**
 * @brief Create I32 response (two registers, signed)
 * @param ptr Pointer to int32_t value
 */
#define MODBUS_RESP_I32(ptr) \
    { .type = MODBUS_TYPE_I32, .value = (ptr), .byte_count = 4, .reg_count = 2, .is_valid = true }

/**
 * @brief Create PACKED response (arbitrary bytes)
 * @param ptr Pointer to data buffer
 * @param bytes Number of bytes to send
 */
#define MODBUS_RESP_PACKED(ptr, bytes) \
    { .type = MODBUS_TYPE_PACKED, .value = (ptr), .byte_count = (bytes), .reg_count = (((bytes) + 1) / 2), .is_valid = true }

/**
 * @brief Create RAW response (raw byte array)
 * @param ptr Pointer to data buffer
 * @param bytes Number of bytes to send
 */
#define MODBUS_RESP_RAW(ptr, bytes) \
    { .type = MODBUS_TYPE_RAW, .value = (ptr), .byte_count = (bytes), .reg_count = (((bytes) + 1) / 2), .is_valid = true }

/**
 * @brief Create STRUCT response (array of registers)
 * @param ptr Pointer to uint16_t array
 * @param regs Number of registers in array
 */
#define MODBUS_RESP_STRUCT(ptr, regs) \
    { .type = MODBUS_TYPE_STRUCT, .value = (ptr), .byte_count = ((regs) * 2), .reg_count = (regs), .is_valid = true }

/**
 * @brief Create invalid response (error/not found)
 */
static inline void ModbusResp_SetInvalid(ModbusDataResponse_t *resp)
{
    if (resp) resp->is_valid = false;
}

/* ============================================================================
 * SERIALIZATION FUNCTIONS (READ OPERATIONS)
 * ============================================================================ */

/**
 * @brief Serialize response data to Modbus buffer
 * 
 * Converts the response structure to big-endian bytes in the Modbus frame.
 * Called internally by process_FC3/process_FC4 after read hooks return.
 * 
 * @param resp Pointer to response structure (filled by read hook)
 * @param buffer Pointer to Modbus frame buffer
 * @param buffer_offset Offset in buffer where data should start (typically 3)
 * @return Number of bytes written, or 0 on error
 * 
 * @note Buffer must have enough space for the response
 * @note All multi-byte values are serialized as big-endian (Modbus standard)
 */
uint8_t ModbusData_SerializeResponse(const ModbusDataResponse_t *resp, 
                                      uint8_t *buffer, 
                                      uint16_t buffer_offset);

/**
 * @brief Serialize U16 value to buffer
 * 
 * @param value 16-bit value to serialize
 * @param buffer Output buffer (must have 2 bytes available)
 * @return Number of bytes written (always 2)
 */
uint8_t ModbusData_SerializeU16(uint16_t value, uint8_t *buffer);

/**
 * @brief Serialize U32 value to buffer
 * 
 * @param value 32-bit value to serialize
 * @param buffer Output buffer (must have 4 bytes available)
 * @return Number of bytes written (always 4)
 */
uint8_t ModbusData_SerializeU32(uint32_t value, uint8_t *buffer);

/* ============================================================================
 * DESERIALIZATION FUNCTIONS (WRITE OPERATIONS)
 * ============================================================================ */

/**
 * @brief Read U16 from buffer (big-endian)
 * 
 * @param buf Pointer to byte buffer
 * @param offset Offset in buffer
 * @return 16-bit value
 */
static inline uint16_t ModbusData_ReadU16(const uint8_t *buf, uint16_t offset)
{
    return ((uint16_t)buf[offset] << 8) | buf[offset + 1];
}

/**
 * @brief Read U32 from buffer (big-endian)
 * 
 * @param buf Pointer to byte buffer
 * @param offset Offset in buffer
 * @return 32-bit value
 */
static inline uint32_t ModbusData_ReadU32(const uint8_t *buf, uint16_t offset)
{
    return ((uint32_t)ModbusData_ReadU16(buf, offset) << 16) | 
            ModbusData_ReadU16(buf, offset + 2);
}

/**
 * @brief Read I32 from buffer (big-endian, signed)
 * 
 * @param buf Pointer to byte buffer
 * @param offset Offset in buffer
 * @return 32-bit signed value
 */
static inline int32_t ModbusData_ReadI32(const uint8_t *buf, uint16_t offset)
{
    return (int32_t)ModbusData_ReadU32(buf, offset);
}

/**
 * @brief Parse U16 from write request
 * 
 * @param req Pointer to write request structure
 * @param out_value Pointer to store parsed value
 * @return true on success, false on error (invalid request/size)
 */
bool ModbusData_ParseU16(const ModbusWriteRequest_t *req, uint16_t *out_value);

/**
 * @brief Parse U32 from write request
 * 
 * @param req Pointer to write request structure
 * @param out_value Pointer to store parsed value
 * @return true on success, false on error (invalid request/size)
 */
bool ModbusData_ParseU32(const ModbusWriteRequest_t *req, uint32_t *out_value);

/**
 * @brief Parse I32 from write request
 * 
 * @param req Pointer to write request structure
 * @param out_value Pointer to store parsed value
 * @return true on success, false on error (invalid request/size)
 */
bool ModbusData_ParseI32(const ModbusWriteRequest_t *req, int32_t *out_value);

/**
 * @brief Copy bytes from write request
 * 
 * @param req Pointer to write request structure
 * @param offset Offset in request data
 * @param dst Destination buffer
 * @param len Number of bytes to copy
 * @return true on success, false on error (offset+len > byte_count)
 */
bool ModbusData_ParseBytes(const ModbusWriteRequest_t *req, 
                           uint16_t offset, 
                           uint8_t *dst, 
                           uint16_t len);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

/**
 * @brief Calculate register count from byte count
 * 
 * @param byte_count Number of bytes
 * @return Number of 16-bit registers needed (rounded up)
 */
static inline uint16_t ModbusData_BytesToRegs(uint16_t byte_count)
{
    return (byte_count + 1) / 2;
}

/**
 * @brief Calculate byte count from register count
 * 
 * @param reg_count Number of registers
 * @return Number of bytes (reg_count * 2)
 */
static inline uint16_t ModbusData_RegsToBytes(uint16_t reg_count)
{
    return reg_count * 2;
}

/* ============================================================================
 * END OF HEADER
 * ============================================================================ */

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_DATA_H */