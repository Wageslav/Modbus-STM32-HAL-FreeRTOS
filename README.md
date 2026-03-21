# Modbus Library for STM32 with FreeRTOS

**Flexible, Universal Modbus RTU/TCP Library with Platform Abstraction Layer (PAL), Handler Registry, and Data Serialization**

---

## 📖 Overview

This is a **refactored fork** of the [alejoseb/Modbus-STM32-HAL-FreeRTOS](https://github.com/alejoseb/Modbus-STM32-HAL-FreeRTOS) library. The original library provides solid Modbus RTU/TCP support for STM32 with FreeRTOS, but it uses a **flat register array** approach that doesn't scale well for complex applications.

### 🔧 What We Changed

| Component | Original | Refactored |
|-----------|----------|------------|
| **Data Access** | Direct `u16regs[]` array access | Flexible hooks with serialization layer |
| **Register Mapping** | Static memory mapping | Dynamic registry with handler callbacks |
| **Data Types** | 16-bit registers only | U16, U32, I32, PACKED, RAW, STRUCT |
| **Hardware Abstraction** | Direct HAL calls | Platform Abstraction Layer (PAL) – works on any STM32 family |
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
| **Portability issues** (HAL version differences) | PAL abstracts UART/GPIO – works on all STM32 families |

---

## 🏗 Architecture

Мы обновили библиотеку, добавили PAL (Platform Abstraction Layer) и успешно протестировали её работу. Теперь нужно обновить `README.md`, чтобы отразить все изменения, предоставить актуальные инструкции по интеграции и добавить раздел с результатами тестирования.

Ниже приведён обновлённый `README.md`. Он включает:

- Уточнение о PAL и его роли.
- Примеры работы (логи из вашего теста).
- Дополнительные инструкции по CMake.
- Раздел "Troubleshooting" с решениями распространённых проблем (включая те, что возникли у нас).
- Актуализацию архитектуры.

```markdown
# Modbus Library for STM32 with FreeRTOS

**Flexible, Universal Modbus RTU/TCP Library with Platform Abstraction Layer (PAL), Handler Registry, and Data Serialization**

---

## 📖 Overview

This is a **refactored fork** of the [alejoseb/Modbus-STM32-HAL-FreeRTOS](https://github.com/alejoseb/Modbus-STM32-HAL-FreeRTOS) library. The original library provides solid Modbus RTU/TCP support for STM32 with FreeRTOS, but it uses a **flat register array** approach that doesn't scale well for complex applications.

### 🔧 What We Changed

| Component | Original | Refactored |
|-----------|----------|------------|
| **Data Access** | Direct `u16regs[]` array access | Flexible hooks with serialization layer |
| **Register Mapping** | Static memory mapping | Dynamic registry with handler callbacks |
| **Data Types** | 16-bit registers only | U16, U32, I32, PACKED, RAW, STRUCT |
| **Hardware Abstraction** | Direct HAL calls | Platform Abstraction Layer (PAL) – works on any STM32 family |
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
| **Portability issues** (HAL version differences) | PAL abstracts UART/GPIO – works on all STM32 families |

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

### Platform Abstraction Layer (PAL)

The library now uses a **Platform Abstraction Layer** to separate hardware‑specific code from the core logic. This allows the same library to work on any STM32 family (F1, F4, G4, H7, etc.) and even other MCUs (with proper port layer). All HAL calls are encapsulated in `ports/modbus_port.h` and implemented per platform.

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
- ✅ **Platform Abstraction Layer** – Works on all STM32 families without modification
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
git submodule add https://github.com/Wageslav/Modbus-STM32-HAL-FreeRTOS.git Middlewares/Modbus
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
    /* Create abstract handles (required by PAL) */
    static ModbusUartHandle_t uart_handle = { .handle = &huart1 };
    static ModbusGpioPort_t gpio_port = { .port = RS485_DE_GPIO_Port };

    ModbusH.uModbusType = MB_SLAVE;
    ModbusH.port = &uart_handle;          /* Abstract UART handle */
    ModbusH.u8id = MODBUS_SLAVE_ID;
    ModbusH.u16timeOut = 1000;
    ModbusH.EN_Port = &gpio_port;         /* Abstract GPIO port */
    ModbusH.EN_Pin = RS485_DE_Pin;
    ModbusH.u16regs = ModbusDATA;         /* fallback if hooks not set */
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
```

### 6. CMake Integration

Add the following to your `CMakeLists.txt`:

```cmake
# Include paths
target_include_directories(your_target PRIVATE
    ${CMAKE_SOURCE_DIR}/Config
    ${CMAKE_SOURCE_DIR}/Middlewares/Modbus/MODBUS-LIB/Inc
    ${CMAKE_SOURCE_DIR}/Middlewares/Modbus/MODBUS-LIB/Inc/extensions
    ${CMAKE_SOURCE_DIR}/Middlewares/Modbus/MODBUS-LIB/Inc/ports
    ${CMAKE_SOURCE_DIR}/Middlewares/Modbus/MODBUS-LIB/Inc/ports/stm32
)

# Source files
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

The library has been tested on **STM32G474** with the following configuration:
- Modbus RTU Slave, ID=1, baudrate=115200
- RS485 transceiver with DE/RE control
- Handlers for U16, U32, and packed data

### Sample Logs

```
18:25:20.46 -> 01 03 00 00 00 06 C5 C8 
18:25:20.451 <- 01 03 0C 03 E8 10 01 00 00 00 00 00 00 00 00 01 00 00 00 01 00 00 00 BC 5F

[20044] [INFO ] Modbus: reads=1, writes=0, errors=0
[25048] [INFO ] System alive: 25 seconds
[25052] [INFO ] Modbus: reads=1, writes=0, errors=0
[115192] [INFO ] System alive: 115 seconds
[115196] [INFO ] Modbus: reads=2, writes=1, errors=0

18:27:56.57 -> 01 06 00 00 12 34 84 BD 
18:27:56.189 <- 01 06 00 00 12 34 84 BD  [CRC OK]

18:28:02.889 -> 01 03 00 00 00 01 84 0A 
18:28:03.14 <- 01 03 02 12 34 B5 33  [CRC OK]
```

The logs show successful:
- Read of 6 registers (response with correct CRC)
- Write to register 0x0000 with value 0x1234
- Read back of that register (value 0x1234)

---

## 🔧 Troubleshooting

### Common Issues and Solutions

| Issue | Likely Cause | Solution |
|-------|--------------|----------|
| **`HAL_UART_State_t` not found** | HAL version differences | Use `uint32_t` for state; library now uses PAL to avoid this |
| **`xSemaphoreTake` implicit declaration** | Using CMSIS-RTOS semaphore | Use `osSemaphoreAcquire` instead (fixed in PAL version) |
| **Incompatible pointer types** | Passing HAL handle directly | Use abstract wrappers: `ModbusUartHandle_t` and `ModbusGpioPort_t` |
| **No response from slave** | Handlers not attached | Call `ModbusHandler_Attach()` after `ModbusInit()` |
| **Data returned is wrong** | `static` variable missing in handler | Use `static` to keep pointer valid after return |
| **Timeout errors** | Handler execution >1ms | Queue heavy work to background task |
| **Stack overflow** | Insufficient task stack | Increase `stack_size` in task attributes (e.g., 512*4) |

### Debugging

Enable debug output in `ModbusConfig.h`:

```c
#define MODBUS_DEBUG_ENABLED 1
```

Then use:

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
│   └── ModbusConfig.h              ← Your configuration (copy from template)
│
├── Middlewares/
│   └── Modbus/                     ← SUBMODULE (do not modify)
│       └── MODBUS-LIB/
│           ├── Config/
│           │   └── ModbusConfigTemplate.h  ← Template (reference only)
│           ├── Inc/
│           │   ├── Modbus.h
│           │   ├── ports/
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
```
