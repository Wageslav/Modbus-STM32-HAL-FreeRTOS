# Modbus Library for STM32 with FreeRTOS

**Flexible, Universal Modbus RTU/TCP Library with Handler Registry and Data Serialization**

---

## 📖 Overview

This is a **refactored fork** of the [alejoseb/Modbus-STM32-HAL-FreeRTOS](https://github.com/alejoseb/Modbus-STM32-HAL-FreeRTOS) library. The original library provides solid Modbus RTU/TCP support for STM32 with FreeRTOS, but it uses a **flat register array** approach that doesn't scale well for complex applications.

### 🔧 What We Changed

| Component | Original | Refactored |
|-----------|----------|------------|
| **Data Access** | Direct `u16regs[]` array access | Flexible hooks with serialization layer |
| **Register Mapping** | Static memory mapping | Dynamic registry with handler callbacks |
| **Data Types** | 16-bit registers only | U16, U32, I32, PACKED, RAW, STRUCT |
| **Configuration** | In-library `ModbusConfig.h` | Project-specific config (submodule stays clean) |
| **Integration** | Manual register synchronization | Handler API bridge with automatic dispatch |
| **Architecture** | Monolithic | Modular (Core + Extensions + Ports) |

### 🎯 Why We Changed It

| Problem | Solution |
|---------|----------|
| **Cannot send structures** (e.g., `union { int32_t weight; uint8_t status; }`) | `ModbusDataResponse_t` with PACKED/RAW types |
| **Commands require polling** (write register → check in main loop) | Write hooks execute logic immediately |
| **Address calculation is manual** (motor ID → register offset) | Registry handles address ranges with context |
| **Submodule gets modified** (project-specific config in lib) | Config lives in project, lib stays universal |
| **No type safety** (raw `uint16_t*` everywhere) | Typed serialization/deserialization helpers |
| **Hard to test** (handlers coupled to library) | Handlers are independent, testable without Modbus |

---

## 🏗 Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    YOUR PROJECT (Core/)                         │
├─────────────────────────────────────────────────────────────────┤
│  modbus_app_handlers.c  ← Your business logic handlers          │
│  modbus_app_register.c  ← Register handlers to registry         │
│  Config/ModbusConfig.h  ← Your configuration (not in submodule) │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ uses
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│              MIDDLEWARE (ModbusIntegration/)                    │
├─────────────────────────────────────────────────────────────────┤
│  modbus_handler.c  ← Bridge: library hooks → registry lookup    │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ calls
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│              SUBMODULE (Middlewares/Modbus/)                    │
├─────────────────────────────────────────────────────────────────┤
│  MODBUS-LIB/                                                    │
│  ├── Inc/                                                       │
│  │   ├── Modbus.h              ← Protocol + hook definitions    │
│  │   └── extensions/                                            │
│  │       ├── modbus_data.h     ← Serialization (U16/U32/PACKED) │
│  │       ├── modbus_registry.h ← Registry: address → handler    │
│  │       └── modbus_handler.h  ← Bridge API                     │
│  └── Src/                                                       │
│      ├── Modbus.c              ← Protocol + hook calls          │
│      └── extensions/                                            │
│          ├── modbus_data.c     ← Serialize/Deserialize          │
│          ├── modbus_registry.c ← Register/Lookup                │
│          └── modbus_handler.c  ← Dispatch to handlers           │
└─────────────────────────────────────────────────────────────────┘
```

### Key Principles

1. **Submodule is universal** — No project-specific code in `Middlewares/Modbus/`
2. **Config lives in project** — `Config/ModbusConfig.h` overrides template
3. **Handlers are independent** — Test without Modbus, no library includes
4. **Clean boundaries** — Library doesn't know about your structures

---

## 📦 Features

### Core Features (from original)
- ✅ Modbus RTU over USART (interrupt + DMA)
- ✅ Modbus TCP (server + client)
- ✅ Modbus USB-CDC
- ✅ FreeRTOS-based (task, semaphore, timers)
- ✅ Master and Slave modes
- ✅ Multiple concurrent instances
- ✅ RS485 direction control

### New Features (refactored)
- ✅ **Flexible Data Layer** — Serialize U16, U32, I32, PACKED, RAW, STRUCT
- ✅ **Handler Registry** — Dynamic registration of read/write handlers
- ✅ **Handler API Bridge** — Automatic dispatch from library to your handlers
- ✅ **Access Levels** — RELEASE, SERVICE, DEBUG (compile-time or runtime)
- ✅ **Address Ranges** — Register ranges with context (e.g., motor ID)
- ✅ **Project Config** — Configuration in project, not submodule
- ✅ **Thread-Safe** — Mutex with timeout for shared data access
- ✅ **Statistics** — Debug counters for read/write operations

---

## 🚀 Quick Start

### 1. Add Submodule

```bash
cd your-project/
git submodule add 
https://github.com/Wageslav/Modbus-STM32-HAL-FreeRTOS.git Middlewares/Modbus
git submodule update --init --recursive
```

### 2. Copy Configuration

```bash
cp Middlewares/Modbus/MODBUS-LIB/Config/ModbusConfigTemplate.h Config/ModbusConfig.h
```

Edit `Config/ModbusConfig.h`:

```c
#define MODBUS_DATA_LAYER_ENABLED     1
#define MODBUS_REGISTRY_ENABLED       1
#define MODBUS_HANDLER_API_ENABLED    1
#define MODBUS_SYNC_ENABLED           1
#define MAX_REGISTRY_ENTRIES          256
```

### 3. Create Handlers

**Core/Src/modbus_app_handlers.c**:

```c
#include "modbus_data.h"      /* from submodule */
#include "profile_manager.h"  /* your project */

/* Read handler: 16-bit parameter */
static bool read_basket_opening_time(uint16_t addr, 
                                      ModbusDataResponse_t *resp, 
                                      void *ctx)
{
    (void)addr; (void)ctx;
    
    static uint16_t value;  /* static! pointer must remain valid */
    value = Profile_GetParamValue(MOTOR_L_PROFILE_ID, 
                                   PROFILE_PARAM_TARGET_DURATION_MS) / 10;
    
    *resp = MODBUS_RESP_U16(&value);
    return true;
}

/* Write handler: command trigger */
static bool cmd_basket_open(uint16_t addr, 
                             const ModbusWriteRequest_t *req, 
                             void *ctx)
{
    (void)addr; (void)req; (void)ctx;
    
    Init_Profile_Table();
    ScenarioScheduler_StartScenarioById(MOTOR_L_BASKET_OPEN);
    return true;
}

/* Read handler: packed data (weight + status) */
static bool read_current_weight(uint16_t addr, 
                                 ModbusDataResponse_t *resp, 
                                 void *ctx)
{
    (void)addr; (void)ctx;
    
    static union {
        int32_t weight;
        uint8_t bytes[4];
    } packed;
    
    packed.weight = (int32_t)scale.output_weight_f;
    packed.bytes[3] = scale_get_combined_status();
    
    *resp = MODBUS_RESP_PACKED(&packed, 4);  /* 4 bytes = 2 registers */
    return true;
}
```

### 4. Register Handlers

**Core/Src/modbus_app_register.c**:

```c
#include "modbus_registry.h"
#include "modbus_app_handlers.h"

static const ModbusRegDescriptor_t app_regs[] = {
    /* Address, End, Type, Level, Read Hook, Write Hook, Context */
    
    /* Parameters (RW) */
    {0xAAA0, 0xAAA0, MODBUS_REG_SIMPLE, MODBUS_ACCESS_RELEASE,
     read_basket_opening_time, write_basket_opening_time, NULL},
    
    /* Commands (WO) */
    {0xAAAB, 0xAAAB, MODBUS_REG_CMD, MODBUS_ACCESS_RELEASE,
     NULL, cmd_basket_open, NULL},
    
    /* Packed data (RO) */
    {0xAA88, 0xAA89, MODBUS_REG_PACKED, MODBUS_ACCESS_RELEASE,
     read_current_weight, NULL, NULL},
};

void ModbusApp_RegisterHandlers(void)
{
    ModbusRegistry_Init();
    
    size_t registered = ModbusRegistry_RegisterMany(
        app_regs, 
        sizeof(app_regs) / sizeof(app_regs[0])
    );
    
    LOG_INFO("Modbus: Registered %zu handlers", registered);
}
```

### 5. Initialize in main.c

```c
#include "Modbus.h"           /* from submodule */
#include "modbus_handler.h"   /* from submodule */
#include "modbus_app_register.h" /* your project */

static void Modbus_System_Init(void)
{
    /* 1. Initialize library */
    ModbusH.uModbusType = MB_SLAVE;
    ModbusH.port = &huart1;
    ModbusH.u8id = MODBUS_SLAVE_ID;
    ModbusH.u16timeOut = 1000;
    ModbusH.EN_Port = RS485_DE_GPIO_Port;
    ModbusH.EN_Pin = RS485_DE_Pin;
    ModbusH.u16regs = ModbusDATA;  /* fallback if hooks not set */
    ModbusH.u16regsize = MODBUS_REGISTER_COUNT;
    ModbusH.xTypeHW = USART_HW;
    
    ModbusInit(&ModbusH);
    
    /* 2. Register YOUR handlers */
    ModbusApp_RegisterHandlers();
    
    /* 3. Attach handler bridge to library */
    ModbusHandler_Attach(&ModbusH, NULL);
    
    /* 4. Start */
    ModbusStart(&ModbusH);
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    
    osKernelInitialize();
    
    Modbus_System_Init();
    
    defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
    
    osKernelStart();
    
    while (1) {}
}
```

---

## 📊 Data Types

### Supported Types

| Type | Macro | Registers | Bytes | Use Case |
|------|-------|-----------|-------|----------|
| **U16** | `MODBUS_RESP_U16(ptr)` | 1 | 2 | Simple parameters |
| **U32** | `MODBUS_RESP_U32(ptr)` | 2 | 4 | Counters, timestamps |
| **I32** | `MODBUS_RESP_I32(ptr)` | 2 | 4 | Signed values (temperature) |
| **PACKED** | `MODBUS_RESP_PACKED(ptr, bytes)` | N | N | Unions, mixed types |
| **RAW** | `MODBUS_RESP_RAW(ptr, bytes)` | N | N | Strings, blobs |
| **STRUCT** | `MODBUS_RESP_STRUCT(ptr, regs)` | N | N*2 | Arrays of registers |

### Example: Packed Data

```c
/* Weight (int32) + Status (uint8) in 4 bytes */
static bool read_weight_with_status(uint16_t addr, 
                                     ModbusDataResponse_t *resp, 
                                     void *ctx)
{
    static union {
        int32_t weight;
        uint8_t bytes[4];
    } packed;
    
    packed.weight = scale.output_weight_f;
    packed.bytes[3] = scale_get_status();  /* status in LSB */
    
    *resp = MODBUS_RESP_PACKED(&packed, 4);
    return true;
}
```

**Modbus Response** (FC3, 2 registers):
```
[SlaveID][03][04][Weight Hi][Weight Lo][Status][Pad][CRC]
```

---

## 🔐 Access Levels

### Levels

| Level | Value | Description |
|-------|-------|-------------|
| **RELEASE** | 0 | Production access (always allowed) |
| **SERVICE** | 1 | Service/maintenance access |
| **DEBUG** | 2 | Debug/development access |

### Runtime Control

```c
/* Set access level (e.g., after authentication) */
ModbusHandler_SetAccessLevel(MODBUS_ACCESS_SERVICE);

/* Get current level */
uint8_t level = ModbusHandler_GetAccessLevel();
```

### Register Definition

```c
{0xDB00, 0xDB00, MODBUS_REG_SIMPLE, MODBUS_ACCESS_DEBUG,
 read_debug_param, write_debug_param, NULL},
```

---

## 🧪 Testing

### Modbus Client Commands

```bash
# Read 16-bit parameter (FC3)
modbus_client read 1 3 0xAAA0 1

# Write 16-bit parameter (FC6)
modbus_client write 1 6 0xAAA0 100

# Execute command (FC6, value ignored)
modbus_client write 1 6 0xAAAB 1

# Read packed data (FC3, 2 registers = 4 bytes)
modbus_client read 1 3 0xAA88 2

# Read variable length (FC3, 32 registers = 64 bytes)
modbus_client read 1 3 0xEA00 32

# Write multiple registers (FC16)
modbus_client write 1 16 0xDB00 9 00 00 03 E8 ...
```

### Debug Statistics

```c
#if MODBUS_DEBUG_ENABLED == 1
ModbusHandlerStats_t stats;
ModbusHandler_GetStats(&stats);

LOG_INFO("Read operations: %lu", stats.read_count);
LOG_INFO("Write operations: %lu", stats.write_count);
LOG_INFO("Access denied: %lu", stats.access_denied);
LOG_INFO("Not found: %lu", stats.not_found);
#endif
```

### Stack Monitoring

```c
/* In Modbus task or periodic check */
UBaseType_t watermark = uxTaskGetStackHighWaterMark(ModbusH.myTaskModbusAHandle);
LOG_INFO("Modbus stack watermark: %lu words", watermark);

/* Recommended: 512 * 4 bytes minimum */
```

---

## 🔧 Maintenance

### Updating Submodule

```bash
# Pull latest changes from your fork
cd Middlewares/Modbus
git pull origin main

# Return to project root
cd ../..

# Commit submodule update
git add Middlewares/Modbus
git commit -m "Update Modbus submodule"
```

### Adding New Registers

1. **Add handler** in `modbus_app_handlers.c`
2. **Add entry** in `modbus_app_register.c`
3. **Rebuild** — no library changes needed

### Debugging

| Issue | Check |
|-------|-------|
| **No response** | Verify `ModbusHandler_Attach()` called after `ModbusInit()` |
| **Wrong data** | Check `static` variables in handlers (pointer must remain valid) |
| **Timeout** | Ensure handlers execute < 1ms (queue heavy work to background task) |
| **Access denied** | Verify `ModbusHandler_SetAccessLevel()` called |
| **Stack overflow** | Increase Modbus task stack to `512 * 4` |

### Best Practices

1. **Handlers should be fast** — < 1ms execution time
2. **Use static variables** — Data pointers must remain valid after return
3. **Queue heavy work** — For complex operations, queue to background task
4. **Check return values** — `ModbusRegistry_Register()` can fail (full, overlap)
5. **Monitor stack** — Use `uxTaskGetStackHighWaterMark()` periodically

---

## 📁 File Structure

```
your-project/
├── Config/
│   └── ModbusConfig.h              ← Your configuration (copy from template)
│
├── Middlewares/
│   └── Modbus/                     ← SUBMODULE (do not modify)
│       └── MODBUS-LIB/
│           ├── Config/
│           │   └── ModbusConfigTemplate.h  ← Template (reference only)
│           ├── Inc/
│           │   ├── Modbus.h
│           │   └── extensions/
│           │       ├── modbus_data.h
│           │       ├── modbus_registry.h
│           │       └── modbus_handler.h
│           └── Src/
│               ├── Modbus.c
│               └── extensions/
│                   ├── modbus_data.c
│                   ├── modbus_registry.c
│                   └── modbus_handler.c
│
├── Core/
│   └── Src/
│       ├── modbus_app_handlers.c   ← Your handlers
│       ├── modbus_app_register.c   ← Your registration
│       └── main.c                  ← Initialization
│
└── CMakeLists.txt                  ← Include paths + sources
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

---
