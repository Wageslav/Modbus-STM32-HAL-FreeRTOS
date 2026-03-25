```markdown
# Modbus Library for STM32 with FreeRTOS

**Flexible, Universal Modbus RTU Library with Platform Abstraction Layer (PAL), Dynamic Handler Registry, and Custom Broadcast Protocol**

---

## 📖 Overview

This is a **heavily refactored fork** of the [alejoseb/Modbus-STM32-HAL-FreeRTOS](https://github.com/alejoseb/Modbus-STM32-HAL-FreeRTOS) library. The original library uses a **flat register array** (`u16regs`) that forces continuous memory allocation, making it unsuitable for sparse register maps and complex applications.  

We have transformed it into a **modern, modular Modbus stack** that supports:

- **Virtual Device** – multiple physical devices with the same Slave ID on one RS-485 bus  
- **Sparse Register Maps** – any addresses (e.g., `0xEAA1`) without allocating large arrays  
- **Custom Broadcast Protocol** – FC6 with 24‑bit mask to selectively address groups of devices  
- **Arbitrary Data Types** – U16, U32, I32, packed structures, raw bytes  
- **Platform Abstraction Layer** – works on any STM32 family (G4, F4, H7, etc.) without modifying the library  
- **Zero-Copy Serialization** – handlers fill static structures; library serializes them directly into the Modbus frame  

---

## 🔧 What We Changed

| Component | Original | Refactored |
|-----------|----------|------------|
| **Register Access** | Direct `u16regs[]` array | **Handler Registry** – dynamic mapping of addresses to functions |
| **Address Model** | Continuous from 0 to max address | **Sparse** – only registered addresses exist |
| **Data Types** | Only 16‑bit integers | **U16, U32, I32, PACKED, RAW, STRUCT** |
| **Broadcast** | Standard (all devices respond) | **Custom FC6 with 24‑bit mask** – selective group control |
| **Hardware Abstraction** | Direct HAL calls | **PAL** – abstract UART/GPIO types, easy porting |
| **Configuration** | In‑library `ModbusConfig.h` | **Project‑owned config** – library stays clean as submodule |
| **Integration** | Manual register sync | **Automatic dispatch** via `onReadFlex`/`onWriteFlex` hooks |
| **Memory Footprint** | `(max_address+1) * 2` bytes | **Only registry table** – no wasted memory |

---

## 🎯 Why We Changed It

| Problem | Solution |
|---------|----------|
| **Sparse addresses impossible** (e.g., `0xEAA1`) | Registry stores only registered addresses |
| **Multiple devices with same ID conflict** | Virtual device mode: respond only to own registers, ignore others |
| **Need to send 32‑bit values or structures** | `ModbusDataResponse_t` with `PACKED` type |
| **Commands require polling in main loop** | Write hooks execute logic immediately |
| **Submodule gets modified per project** | Configuration moved to project, library unchanged |
| **No type safety** | Typed serialization/deserialization |
| **Hard to test handlers** | Handlers independent from library |

---

## 🏗 Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    YOUR PROJECT (Core/)                         │
├─────────────────────────────────────────────────────────────────┤
│  modbus_app_handlers.c   ← Business logic handlers              │
│  modbus_app_register.c   ← Register handlers to registry        │
│  Config/ModbusConfig.h   ← Your configuration                   │
└─────────────────────────────────────────────────────────────────┘
                              │ uses
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│              MIDDLEWARE (ModbusIntegration/)                    │
├─────────────────────────────────────────────────────────────────┤
│  modbus_handler.c   ← Bridge: library hooks → registry lookup   │
└─────────────────────────────────────────────────────────────────┘
                              │ calls
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│              SUBMODULE (Middlewares/Modbus/MODBUS-LIB/)         │
├─────────────────────────────────────────────────────────────────┤
│  Core: Modbus.c, Modbus.h                                       │
│  Extensions: modbus_data.c, modbus_registry.c, modbus_handler.c│
│  Ports: modbus_port_stm32.c (abstracts UART/GPIO)              │
└─────────────────────────────────────────────────────────────────┘
```

### Platform Abstraction Layer (PAL)

All hardware‑specific code is isolated in `ports/`.  
To port to a new STM32 family (or even a different MCU), you only need to implement:

- `ModbusPort_UartGetState()`  
- `ModbusPort_UartReceive_IT()` / `AbortReceive_IT()`  
- `ModbusPort_UartTransmit_IT()`  
- `ModbusPort_GpioWrite()`  
- `ModbusPort_GetTick()` / `DelayMs()`  
- `ModbusPort_EnterCritical()` / `ExitCritical()`

No changes to the core library are required.

---

## 📦 Features

### Core Features (from original)
- Modbus RTU over USART (interrupt mode)  
- FreeRTOS‑based (task, semaphore, timers)  
- Slave mode  
- RS485 direction control  

### New Features (refactored)
- ✅ **Dynamic Handler Registry** – map any address to your handler  
- ✅ **Virtual Device Mode** – ignore requests not in your handler table  
- ✅ **Custom Broadcast Protocol** – FC6 with 24‑bit mask (byte order: `[cmd][mask[2]][mask[1]][mask[0]]`)  
- ✅ **Flexible Data Layer** – U16, U32, I32, PACKED, RAW, STRUCT serialization  
- ✅ **Zero‑Copy** – handlers fill static buffers, library serializes directly  
- ✅ **Thread‑Safe Mutex** – protect shared data with timeout  
- ✅ **Access Levels** – RELEASE, SERVICE, DEBUG  
- ✅ **Debug Statistics** – read/write counters, error counters  
- ✅ **Platform Abstraction Layer** – works on STM32G4, F4, H7, etc.  

---

## 🚀 Quick Start

### 1. Add Submodule

```bash
cd your-project/
git submodule add https://github.com/Wageslav/Modbus-STM32-HAL-FreeRTOS.git Middlewares/Modbus
git submodule update --init --recursive
```

### 2. Copy Configuration Template

```bash
cp Middlewares/Modbus/MODBUS-LIB/Config/ModbusConfigTemplate.h Config/ModbusConfig.h
```

Edit `Config/ModbusConfig.h` – enable required features:

```c
#define MODBUS_DATA_LAYER_ENABLED       1
#define MODBUS_REGISTRY_ENABLED         1
#define MODBUS_HANDLER_API_ENABLED      1
#define MODBUS_SYNC_ENABLED             1
#define MODBUS_CUSTOM_BROADCAST_ENABLED 1   /* if using custom FC6 broadcast */
#define MAX_REGISTRY_ENTRIES            256
```

### 3. Create Handlers

**Core/Src/modbus_app_handlers.c**:

```c
#include "modbus_data.h"
#include "modbus_sync.h"

/* Example: read 16-bit parameter */
static ModbusResult_t read_speed(uint16_t addr, ModbusDataResponse_t *resp, void *ctx)
{
    (void)addr; (void)ctx;
    if (!ModbusSync_TakeMutex(100)) return MB_RESULT_EXCEPTION;

    static uint16_t value;
    value = motor_get_speed();
    ModbusSync_GiveMutex();

    *resp = MODBUS_RESP_U16(&value);
    return MB_RESULT_OK;
}

/* Example: write command (no response) */
static ModbusResult_t cmd_start(uint16_t addr, const ModbusWriteRequest_t *req, void *ctx)
{
    (void)addr; (void)req; (void)ctx;
    motor_start();
    return MB_RESULT_OK;   /* or MB_RESULT_SILENT if you don't want response */
}
```

### 4. Register Handlers

**Core/Src/modbus_app_register.c**:

```c
#include "modbus_registry.h"
#include "modbus_app_handlers.h"

static const ModbusRegDescriptor_t app_regs[] = {
    /* address, end, type, access, read_hook, write_hook, context */
    {0x1000, 0x1000, MODBUS_REG_SIMPLE, MODBUS_ACCESS_RELEASE,
     read_speed, write_speed, NULL},
    {0x2000, 0x2000, MODBUS_REG_CMD, MODBUS_ACCESS_RELEASE,
     NULL, cmd_start, NULL},
};

void ModbusApp_RegisterHandlers(void)
{
    ModbusRegistry_Init();
    size_t reg = ModbusRegistry_RegisterMany(app_regs, sizeof(app_regs)/sizeof(app_regs[0]));
    LOG_INFO("Registered %zu handlers", reg);
}
```

### 5. Define Broadcast Commands (if using custom protocol)

**Core/Src/modbus_app_handlers.c** – add:

```c
#include "modbus_registry.h"

/* Helper: check 24-bit mask */
static bool is_targeted_by_mask(const ModbusWriteRequest_t *req)
{
    if (!req->is_broadcast) return true;
    if (req->byte_count < 4) return false;
    uint32_t mask = ((uint32_t)req->data[1] << 16) |
                    ((uint32_t)req->data[2] << 8)  |
                    ((uint32_t)req->data[3]);
    return (mask & (1 << (g_slave_id - 1))) != 0;
}

static ModbusResult_t handle_broadcast_cmd_work_cycle_basket(uint16_t addr, const ModbusWriteRequest_t *req, void *ctx)
{
    (void)addr; (void)ctx;
    if (!is_targeted_by_mask(req)) return MB_RESULT_SILENT;
    motor_start_basket_cycle();
    return MB_RESULT_SILENT;   /* no response for broadcast */
}

const ModbusBroadcastCommand_t g_broadcast_commands[] = {
    {0xAA, handle_broadcast_cmd_work_cycle_basket, MODBUS_ACCESS_RELEASE},
    {0xBB, handle_broadcast_cmd_work_cycle_dobros, MODBUS_ACCESS_RELEASE},
};
size_t g_broadcast_commands_count = sizeof(g_broadcast_commands) / sizeof(g_broadcast_commands[0]);
```

### 6. Initialize in `main.c`

```c
#include "Modbus.h"
#include "modbus_handler.h"
#include "modbus_app_register.h"
#include "modbus_sync.h"

/* Global slave ID for broadcast mask */
uint8_t g_slave_id;

static void Modbus_System_Init(void)
{
    /* Init sync primitives */
    ModbusSync_Init();

    /* Create abstract handles (PAL) */
    static ModbusUartHandle_t uart_handle = { .handle = &huart1 };
    static ModbusGpioPort_t gpio_port = { .port = RS485_DE_GPIO_Port };

    ModbusH.uModbusType   = MB_SLAVE;
    ModbusH.port          = &uart_handle;
    ModbusH.u8id          = g_slave_id;
    ModbusH.u16timeOut    = TIMEOUT_MODBUS;
    ModbusH.EN_Port       = &gpio_port;
    ModbusH.EN_Pin        = RS485_DE_Pin;
    ModbusH.u16regs       = NULL;          /* no fallback array */
    ModbusH.u16regsize    = 0;
    ModbusH.xTypeHW       = USART_HW;
    ModbusH.dynamic_handlers = false;      /* set true in Attach */

    ModbusInit(&ModbusH);

    /* Register your handlers */
    ModbusApp_RegisterHandlers();

    /* Attach handler bridge (enables dynamic mode) */
    ModbusHandler_Attach(&ModbusH, NULL);

    /* Start Modbus communication */
    ModbusStart(&ModbusH);
}
```

### 7. CMake Integration

Add to your `CMakeLists.txt`:

```cmake
target_include_directories(your_target PRIVATE
    ${CMAKE_SOURCE_DIR}/Config
    ${CMAKE_SOURCE_DIR}/Middlewares/Modbus/MODBUS-LIB/Inc
    ${CMAKE_SOURCE_DIR}/Middlewares/Modbus/MODBUS-LIB/Inc/extensions
    ${CMAKE_SOURCE_DIR}/Middlewares/Modbus/MODBUS-LIB/Inc/ports
    ${CMAKE_SOURCE_DIR}/Middlewares/Modbus/MODBUS-LIB/Inc/ports/stm32
)

target_sources(your_target PRIVATE
    ${CMAKE_SOURCE_DIR}/Middlewares/Modbus/MODBUS-LIB/Src/Modbus.c
    ${CMAKE_SOURCE_DIR}/Middlewares/Modbus/MODBUS-LIB/Src/ports/stm32/modbus_port_stm32.c
    ${CMAKE_SOURCE_DIR}/Middlewares/Modbus/MODBUS-LIB/Src/ports/stm32/modbus_uart_callback.c
    ${CMAKE_SOURCE_DIR}/Middlewares/Modbus/MODBUS-LIB/Src/extensions/modbus_data.c
    ${CMAKE_SOURCE_DIR}/Middlewares/Modbus/MODBUS-LIB/Src/extensions/modbus_registry.c
    ${CMAKE_SOURCE_DIR}/Middlewares/Modbus/MODBUS-LIB/Src/extensions/modbus_handler.c
    ${CMAKE_SOURCE_DIR}/Middlewares/Modbus/MODBUS-LIB/Src/extensions/modbus_sync.c
)
```

---

## ✅ Testing & Validation

The library has been extensively tested on **STM32G474** with the following setup:

- Modbus RTU Slave, ID = 1, baudrate = 115200  
- RS485 transceiver with DE/RE control  
- Handlers for U16, U32, and packed data  
- Custom broadcast protocol (FC6 with 24‑bit mask)  

### Sample Logs (successful exchange)

```
18:27:56.57 -> 01 06 00 00 12 34 84 BD 
18:27:56.189 <- 01 06 00 00 12 34 84 BD   [CRC OK]
18:28:02.889 -> 01 03 00 00 00 01 84 0A 
18:28:03.14  <- 01 03 02 12 34 B5 33      [CRC OK]
```

**System output** (debug logs):

```
[INFO] ADS1232 powered up
[INFO] CAL: Context initialized
[INFO] Calibration system initialized
[INFO] === MODBUS INITIALIZED (DYNAMIC MODE) ===
[INFO] Slave ID: 1
[INFO] Registered handlers: 21
[INFO] === SYSTEM STARTED ===
[INFO] Actuator[0]: DM542E | en=0, homed=1, moving=0, pos=0, steps=0, spd=0/0 Hz
[INFO] Actuator[1]: TB6600HG | en=0, homed=1, moving=0, pos=0, steps=0, spd=0/0 Hz
```

**Broadcast test** (FC6 with mask `0xFFFFFF` to activate all devices):

```
00 06 AA FF FF FF 98 43   (command 0xAA, mask 0xFFFFFF)
```

All devices with ID in 1..24 executed the command and remained silent.

---

## 🔧 Troubleshooting

| Issue | Likely Cause | Solution |
|-------|--------------|----------|
| **No response from slave** | Handlers not attached | Call `ModbusHandler_Attach()` after `ModbusInit()` |
| **Device replies to wrong addresses** | `dynamic_handlers` not set | Ensure `ModbusHandler_Attach()` is called; it sets `dynamic_handlers = true` |
| **Broadcast commands not processed** | `MODBUS_CUSTOM_BROADCAST_ENABLED` not defined | Enable it in `ModbusConfig.h` |
| **Data returned is garbage** | Handler uses non‑static local variable | Use `static` for the data pointer in read handlers |
| **Timeout errors** | Handler execution >1 ms | Queue heavy work to background task; use `MB_RESULT_SILENT` if no response needed |
| **Stack overflow** | Insufficient task stack | Increase `stack_size` in task attributes (e.g., 512*4) |
| **HAL_UART_State_t errors** | HAL version mismatch | PAL solves this; ensure you're using the port layer |
| **Mutex timeout** | Deadlock or long critical section | Increase `MODBUS_MUTEX_TIMEOUT_MS` or check for mutual locking |

### Debugging

Enable debug output in `ModbusConfig.h`:

```c
#define MODBUS_DEBUG_ENABLED 1
```

Then use statistics:

```c
ModbusHandlerStats_t stats;
ModbusHandler_GetStats(&stats);
LOG_INFO("Reads=%lu Writes=%lu Errors=%lu",
         stats.read_count, stats.write_count, stats.read_errors);
```

---

## 📁 File Structure

```
your-project/
├── Config/
│   └── ModbusConfig.h                ← Your configuration
├── Middlewares/
│   └── Modbus/                       ← SUBMODULE (do not modify)
│       └── MODBUS-LIB/
│           ├── Config/
│           │   └── ModbusConfigTemplate.h
│           ├── Inc/
│           │   ├── Modbus.h
│           │   ├── ports/            ← PAL header
│           │   │   ├── modbus_port.h
│           │   │   └── stm32/
│           │   │       └── modbus_port_stm32.h
│           │   └── extensions/
│           │       ├── modbus_data.h
│           │       ├── modbus_registry.h
│           │       └── modbus_handler.h
│           └── Src/
│               ├── Modbus.c
│               ├── ports/
│               │   └── stm32/
│               │       ├── modbus_port_stm32.c
│               │       └── modbus_uart_callback.c
│               └── extensions/
│                   ├── modbus_data.c
│                   ├── modbus_registry.c
│                   └── modbus_handler.c
├── Core/
│   └── Src/
│       ├── modbus_app_handlers.c
│       ├── modbus_app_register.c
│       └── main.c
└── CMakeLists.txt
```

---

## 🤝 Contributing

1. **Fork** the repository  
2. **Create branch** (`feature/your-feature`)  
3. **Implement** with tests  
4. **Submit PR** with description  

### Code Style

- **Comments in English** (for submodule)  
- **Doxygen format** for public APIs  
- **Consistent naming** (`ModbusXxx`, `modbus_xxx`)  

---

## 🙏 Acknowledgments

- Original library: [alejoseb/Modbus-STM32-HAL-FreeRTOS](https://github.com/alejoseb/Modbus-STM32-HAL-FreeRTOS)  
- Based on: [smarmengol/Modbus-Master-Slave-for-Arduino](https://github.com/smarmengol/Modbus-Master-Slave-for-Arduino)  
- This refactor was made possible by the need for **virtual device support**, **sparse registers**, and **custom broadcast protocols** in real industrial projects.
```