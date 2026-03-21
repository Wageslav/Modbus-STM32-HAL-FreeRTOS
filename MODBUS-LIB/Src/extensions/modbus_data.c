/*
 * modbus_data.c
 * 
 * Modbus Data Layer - Generic serialization/deserialization implementation
 * 
 * This module implements type-safe data serialization for Modbus register operations.
 * All multi-byte values are serialized as big-endian (Modbus standard).
 * 
 */

/* ============================================================================
 * INCLUDES
 * ============================================================================ */

#include "modbus_data.h"
#include <string.h>

/* ============================================================================
 * STATIC HELPER FUNCTIONS
 * ============================================================================ */

/**
 * @brief Write 16-bit value to buffer (big-endian)
 * 
 * @param buf Pointer to byte buffer
 * @param offset Offset in buffer
 * @param val 16-bit value to write
 */
static inline void ModbusData_WriteU16_BE(uint8_t *buf, uint16_t offset, uint16_t val)
{
    buf[offset] = (val >> 8) & 0xFF;
    buf[offset + 1] = val & 0xFF;
}

/**
 * @brief Write 32-bit value to buffer (big-endian)
 * 
 * @param buf Pointer to byte buffer
 * @param offset Offset in buffer
 * @param val 32-bit value to write
 */
static inline void ModbusData_WriteU32_BE(uint8_t *buf, uint16_t offset, uint32_t val)
{
    ModbusData_WriteU16_BE(buf, offset,     (val >> 16) & 0xFFFF);
    ModbusData_WriteU16_BE(buf, offset + 2,  val        & 0xFFFF);
}

/* ============================================================================
 * SERIALIZATION IMPLEMENTATION
 * ============================================================================ */

uint8_t ModbusData_SerializeU16(uint16_t value, uint8_t *buffer)
{
    if (buffer == NULL) return 0;
    
    ModbusData_WriteU16_BE(buffer, 0, value);
    return 2;
}

uint8_t ModbusData_SerializeU32(uint32_t value, uint8_t *buffer)
{
    if (buffer == NULL) return 0;
    
    ModbusData_WriteU32_BE(buffer, 0, value);
    return 4;
}

uint8_t ModbusData_SerializeResponse(const ModbusDataResponse_t *resp, 
                                      uint8_t *buffer, 
                                      uint16_t buffer_offset)
{
    /* Validate input parameters */
    if (resp == NULL || buffer == NULL || !resp->is_valid)
    {
        return 0;
    }
    
    /* Validate data pointer */
    if (resp->value == NULL)
    {
        return 0;
    }
    
    /* Validate byte count */
    if (resp->byte_count == 0 || resp->byte_count > MODBUS_DATA_MAX_BYTES)
    {
        return 0;
    }
    
    uint8_t *dst = buffer + buffer_offset;
    uint8_t written = 0;
    
    switch (resp->type)
    {
        case MODBUS_TYPE_U16:
        {
            uint16_t val = *(const uint16_t*)resp->value;
            ModbusData_WriteU16_BE(dst, 0, val);
            written = 2;
            break;
        }
        
        case MODBUS_TYPE_U32:
        case MODBUS_TYPE_I32:
        {
            uint32_t val = *(const uint32_t*)resp->value;
            ModbusData_WriteU32_BE(dst, 0, val);
            written = 4;
            break;
        }
        
        case MODBUS_TYPE_PACKED:
        case MODBUS_TYPE_RAW:
        {
            /* Copy bytes as-is, pad to even number if needed */
            uint16_t bytes = resp->byte_count;
            
            /* Limit to maximum payload size */
            if (bytes > MODBUS_DATA_MAX_BYTES)
            {
                bytes = MODBUS_DATA_MAX_BYTES;
            }
            
            memcpy(dst, resp->value, bytes);
            
            /* Pad to even number of bytes (Modbus register alignment) */
            if (bytes & 1)
            {
                dst[bytes] = 0;
                bytes++;
            }
            
            written = bytes;
            break;
        }
        
        case MODBUS_TYPE_STRUCT:
        {
            /* Array of uint16_t registers */
            uint16_t regs = resp->reg_count;
            const uint16_t *src = (const uint16_t*)resp->value;
            
            for (uint16_t i = 0; i < regs; i++)
            {
                ModbusData_WriteU16_BE(dst + i * 2, 0, src[i]);
            }
            
            written = regs * 2;
            break;
        }
        
        default:
        {
            /* Unknown type */
            return 0;
        }
    }
    
    return written;
}

/* ============================================================================
 * DESERIALIZATION IMPLEMENTATION
 * ============================================================================ */

bool ModbusData_ParseU16(const ModbusWriteRequest_t *req, uint16_t *out_value)
{
    /* Validate input parameters */
    if (req == NULL || out_value == NULL)
    {
        return false;
    }
    
    /* Check minimum size */
    if (req->byte_count < 2)
    {
        return false;
    }
    
    /* Check data pointer */
    if (req->data == NULL)
    {
        return false;
    }
    
    *out_value = ModbusData_ReadU16(req->data, 0);
    return true;
}

bool ModbusData_ParseU32(const ModbusWriteRequest_t *req, uint32_t *out_value)
{
    /* Validate input parameters */
    if (req == NULL || out_value == NULL)
    {
        return false;
    }
    
    /* Check minimum size */
    if (req->byte_count < 4)
    {
        return false;
    }
    
    /* Check data pointer */
    if (req->data == NULL)
    {
        return false;
    }
    
    *out_value = ModbusData_ReadU32(req->data, 0);
    return true;
}

bool ModbusData_ParseI32(const ModbusWriteRequest_t *req, int32_t *out_value)
{
    /* Validate input parameters */
    if (req == NULL || out_value == NULL)
    {
        return false;
    }
    
    /* Check minimum size */
    if (req->byte_count < 4)
    {
        return false;
    }
    
    /* Check data pointer */
    if (req->data == NULL)
    {
        return false;
    }
    
    *out_value = ModbusData_ReadI32(req->data, 0);
    return true;
}

bool ModbusData_ParseBytes(const ModbusWriteRequest_t *req, 
                           uint16_t offset, 
                           uint8_t *dst, 
                           uint16_t len)
{
    /* Validate input parameters */
    if (req == NULL || dst == NULL)
    {
        return false;
    }
    
    /* Check data pointer */
    if (req->data == NULL)
    {
        return false;
    }
    
    /* Check bounds */
    if (offset + len > req->byte_count)
    {
        return false;
    }
    
    memcpy(dst, req->data + offset, len);
    return true;
}

/* ============================================================================
 * END OF FILE
 * ============================================================================ */