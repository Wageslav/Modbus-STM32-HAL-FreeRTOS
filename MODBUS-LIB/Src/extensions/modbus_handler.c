#include "modbus_handler.h"
#include "modbus_registry.h"
#include "modbus_data.h"
#include "Modbus.h"
#include <string.h>

static volatile uint8_t g_access_level = MODBUS_ACCESS_RELEASE;
static void *g_app_context = NULL;
static bool g_attached = false;

#if MODBUS_CUSTOM_BROADCAST_ENABLED == 1
extern const ModbusBroadcastCommand_t g_broadcast_commands[];
extern const size_t g_broadcast_commands_count;
#endif

#if MODBUS_DEBUG_ENABLED == 1
static ModbusHandlerStats_t g_stats = {0};
#define STAT_INC_READ()     g_stats.read_count++
#define STAT_INC_WRITE()    g_stats.write_count++
#define STAT_INC_READ_ERR() g_stats.read_errors++
#define STAT_INC_WRITE_ERR() g_stats.write_errors++
#define STAT_INC_ACCESS()   g_stats.access_denied++
#define STAT_INC_NOTFOUND() g_stats.not_found++
#else
#define STAT_INC_READ()
#define STAT_INC_WRITE()
#define STAT_INC_READ_ERR()
#define STAT_INC_WRITE_ERR()
#define STAT_INC_ACCESS()
#define STAT_INC_NOTFOUND()
#endif

void ModbusHandler_SetAccessLevel(uint8_t level) { g_access_level = level; }
uint8_t ModbusHandler_GetAccessLevel(void) { return g_access_level; }

static inline bool ModbusHandler_CheckAccess(ModbusAccessLevel_t required_level) {
#if MODBUS_HANDLER_CHECK_ACCESS == 1
    return (g_access_level >= required_level);
#else
    (void)required_level;
    return true;
#endif
}

static ModbusResult_t ModbusHandler_ReadHook(uint16_t address, ModbusDataResponse_t *response)
{
    STAT_INC_READ();
    if (response == NULL) return MB_RESULT_EXCEPTION;

    ModbusResp_SetInvalid(response);

    const ModbusRegDescriptor_t *desc = ModbusRegistry_Lookup(address);
    if (desc == NULL) {
        STAT_INC_NOTFOUND();
        return MB_RESULT_SILENT;   // ← not registered → silence
    }

    if (!ModbusHandler_CheckAccess(desc->access_level)) {
        STAT_INC_ACCESS();
        return MB_RESULT_EXCEPTION;
    }

    if (desc->read_hook == NULL) {
        STAT_INC_READ_ERR();
        return MB_RESULT_EXCEPTION;
    }

    ModbusResult_t result = desc->read_hook(address, response, desc->context);
    if (result != MB_RESULT_OK && result != MB_RESULT_SILENT && result != MB_RESULT_EXCEPTION) {
        result = MB_RESULT_EXCEPTION;
    }

    if (result == MB_RESULT_OK && !response->is_valid) {
        STAT_INC_READ_ERR();
        return MB_RESULT_EXCEPTION;
    }

    return result;
}

static ModbusResult_t ModbusHandler_WriteHook(uint16_t address, const ModbusWriteRequest_t *request)
{
    STAT_INC_WRITE();
    if (request == NULL) return MB_RESULT_EXCEPTION;

#if MODBUS_CUSTOM_BROADCAST_ENABLED == 1
    /* Handle custom broadcast commands */
    if (request->is_broadcast && request->byte_count >= 4)
    {
        uint8_t cmd = request->data[0];                         // command code
        // mask is in request->data[1..3]
        for (size_t i = 0; i < g_broadcast_commands_count; i++)
        {
            if (g_broadcast_commands[i].command_code == cmd)
            {
                if (g_access_level >= g_broadcast_commands[i].access_level)
                {
                    return g_broadcast_commands[i].handler(address, request, NULL);
                }
                else
                {
                    return MB_RESULT_EXCEPTION; // access denied
                }
            }
        }
        /* Command not found – silently ignore (no response) */
        return MB_RESULT_SILENT;
    }
#endif

    /* Standard register lookup */
    const ModbusRegDescriptor_t *desc = ModbusRegistry_Lookup(address);
    if (desc == NULL) {
        STAT_INC_NOTFOUND();
        return MB_RESULT_SILENT;   // not registered – silent
    }

    if (!ModbusHandler_CheckAccess(desc->access_level)) {
        STAT_INC_ACCESS();
        return MB_RESULT_EXCEPTION;
    }

    if (desc->write_hook == NULL) {
        STAT_INC_WRITE_ERR();
        return MB_RESULT_EXCEPTION;
    }

    ModbusResult_t result = desc->write_hook(address, request, desc->context);
    if (result != MB_RESULT_OK && result != MB_RESULT_SILENT && result != MB_RESULT_EXCEPTION) {
        result = MB_RESULT_EXCEPTION;
    }

    return result;
}

void ModbusHandler_Init(void)
{
    g_access_level = MODBUS_HANDLER_DEFAULT_ACCESS;
    g_app_context = NULL;
    g_attached = false;
#if MODBUS_DEBUG_ENABLED == 1
    memset(&g_stats, 0, sizeof(g_stats));
#endif
}

void ModbusHandler_Attach(modbusHandler_t *modH, void *appContext)
{
    if (modH == NULL) return;
    g_app_context = appContext;
    modH->onReadFlex = ModbusHandler_ReadHook;
    modH->onWriteFlex = ModbusHandler_WriteHook;
    modH->appContext = appContext;
    modH->dynamic_handlers = true;   // ← enable dynamic mode
    g_attached = true;
}

void ModbusHandler_Detach(modbusHandler_t *modH)
{
    if (modH == NULL) return;
    modH->onReadFlex = NULL;
    modH->onWriteFlex = NULL;
    modH->appContext = NULL;
    g_attached = false;
}

bool ModbusHandler_CheckRange(uint16_t start_addr, uint16_t num_regs)
{
    for (uint16_t i = 0; i < num_regs; i++) {
        if (!ModbusRegistry_IsRegistered(start_addr + i))
            return false;
    }
    return true;
}

#if MODBUS_DEBUG_ENABLED == 1
void ModbusHandler_GetStats(ModbusHandlerStats_t *stats)
{
    if (stats == NULL) return;
    memcpy(stats, &g_stats, sizeof(ModbusHandlerStats_t));
}
void ModbusHandler_ResetStats(void)
{
    memset(&g_stats, 0, sizeof(g_stats));
}
#endif