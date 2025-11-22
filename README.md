# SuperTinyKernel (STK)
A minimalistic C++ thread scheduling kernel for embedded systems.

---

[![MIT License](https://img.shields.io/badge/License-MIT-blue.svg)](https://github.com/dmitrykos/stk/blob/main/LICENSE)
[![Build Status](https://img.shields.io/github/actions/workflow/status/dmitrykos/stk/cmake-test-generic-stm32.yml)](https://github.com/dmitrykos/stk/actions)
[![Test Coverage](https://img.shields.io/badge/coverage-100%25-brightgreen)](https://github.com/dmitrykos/stk)

---

## Overview

**SuperTinyKernel (STK)** provides a lightweight, deterministic thread scheduling layer for bare-metal embedded applications.
It does *not* attempt to be a full Real-Time Operating System (RTOS). Instead, STK focuses on:

* Adding multi-threading into existing bare-metal projects with minimal integration effort
* Maintaining very small code size
* Predictable and deterministic execution
* Portability across multiple MCU families

STK is implemented in C++ with a clean **object-oriented design**, while remaining friendly to C developers:

* No aggressive namespace usage
* No dependency on modern C++ features or STL
* Transparent and readable implementation

If you need RTOS-like concurrency without the overhead of a full RTOS, STK is a strong fit.

It is an [open-source project](https://github.com/dmitrykos/stk), naviage its code for more details.

---

## Quick Start (1 minute)

### 1. Clone repository
```bash
git clone https://github.com/dmitrykos/stk.git
cd stk
```

### 2. Build example for x86 development mode
```bash
cd build/example/project/eclipse/x86
mkdir build && cd build
cmake ..
make
./stk_example_x86
```

### 3. Run on hardware
* Import the STM32, RPI (Raspberry Pico) or NXP example in Eclipse CDT IDE or MCUXpresso IDE
* Build and flash your target MCU

---

## Key Features

| Feature | Description |
|--------|-------------|
| Soft real-time | No strict time slots, cooperative scheduling |
| Hard real-time (`KERNEL_HRT`) | Guaranteed execution window, deadline monitoring |
| Static task model (`KERNEL_STATIC`) | Tasks created once at startup |
| Dynamic task model (`KERNEL_DYNAMIC`) | Tasks can be created and exit at runtime |
| Low-power aware | MCU enters sleep when no task is runnable |
| Critical section API | Basic synchronization primitive |
| Development mode (x86) | Run the same threaded application on Windows |
| Tiny footprint | Minimal code unrelated to scheduling |
| Easy porting | Requires very small BSP surface |

---

## Modes of Operation

### Soft Real-Time (default)
Tasks cooperate using `Sleep()`. Timing is best-effort.

### Hard Real-Time (`KERNEL_HRT`)
* Periodic tasks with strict execution windows
* Kernel enforces deadlines
* Any violation fails the application deterministically

> Use cases: motor control, power electronics, aerospace systems.

### Static vs Dynamic

* `KERNEL_STATIC` – tasks created once at startup, infinite loop
* `KERNEL_DYNAMIC` – tasks may exit, kernel returns to `main()` when done

---

## Hardware Support

### CPU Architectures
* ARM Cortex-M0/M3/M4/M7/M33
* RISC-V RV32I (RV32IMA_ZICSR)
* RISC-V RV32E (RV32EMA_ZICSR) — including very small RAM devices

### Floating-point
* Soft
* Hardware (where available)

---

## Dependencies

* CMSIS (ARM platforms only)
* MCU vendor BSP (NXP, STM, RPI, etc.)

No other libraries required.

---

## Development Mode (x86)

STK includes a **full scheduling emulator** for Windows:

* Run the same embedded application on x86
* Debug threads using Visual Studio or Eclipse
* Perform unit testing without hardware
* Mock or simulate peripherals

---

## Tested Boards

* STM STM32F0DISCOVERY (Cortex-M0)
* STM NUCLEO-F103RB (Cortex-M3)
* NXP FRDM-K66F (Cortex-M4F)
* STM STM32F4DISCOVERY (Cortex-M4F)
* NXP MIMXRT1050 EVKB (Cortex-M7)
* Raspberry Pico 2 W (Cortex-M33 / RISC-V variant)

---

## Building and Running Examples

You can build and run examples **without any hardware** on Windows.

### Required tools (PC development)

For STM32, RPI platforms:

* Eclipse Embedded CDT
* xPack GNU ARM Embedded GCC
* xPack QEMU ARM emulator

For NXP platforms:

* MCUXpresso IDE (includes GCC)

### RISC-V Toolchain

* xPack GNU RISC-V Embedded GCC
* xPack QEMU RISC-V

> If you are targeting only ARM, RISC-V tools are not required.

---

## Examples

Example projects are located in:

```
build/example/project/eclipse
```

Grouped by platform:

* `stm` – STM32, runs on QEMU or hardware
* `rpi` – Raspberry Pico
* `x86` – Windows emulator

### Import into Eclipse CDT

```
File → Import... → Existing Projects into Workspace
Select root directory → build/example/project/eclipse
```

STM32 and Raspberry Pico examples include SDK files in:

```
deps/target
```

### NXP Examples

Located in:

```
build/example/project/nxp-mcuxpresso
```

Compatible with:

* Kinetis® K66
* Kinetis® K26
* i.MX RT1050
* other compatible ARM Cortex-M0/M3/M4/M7/M33/... NXP MCUs

---

## Example Code

Below example toggles RGB LEDs on a development board with LED. Each LED is controlled by its own thread, switching at 1s intervals:

```cpp
#include <stk_config.h>
#include <stk.h>
#include "example.h"

static volatile uint8_t g_TaskSwitch = 0;

template <stk::EAccessMode _AccessMode>
class MyTask : public stk::Task<256, _AccessMode>
{
    uint8_t m_taskId;

public:
    MyTask(uint8_t taskId) : m_taskId(taskId) {}
    stk::RunFuncType GetFunc() { return &Run; }
    void *GetFuncUserData() { return this; }

private:
    static void Run(void *user_data)
    {
        ((MyTask *)user_data)->RunInner();
    }

    void RunInner()
    {
        uint8_t task_id = m_taskId;

        while (true)
        {
            if (g_TaskSwitch != task_id)
            {
                stk::Sleep(10);
                continue;
            }

            switch (task_id)
            {
            case 0:
                LED_SET_STATE(LED_RED, true);
                LED_SET_STATE(LED_GREEN, false);
                LED_SET_STATE(LED_BLUE, false);
                break;
            case 1:
                LED_SET_STATE(LED_RED, false);
                LED_SET_STATE(LED_GREEN, true);
                LED_SET_STATE(LED_BLUE, false);
                break;
            case 2:
                LED_SET_STATE(LED_RED, false);
                LED_SET_STATE(LED_GREEN, false);
                LED_SET_STATE(LED_BLUE, true);
                break;
            }

            stk::Sleep(1000);
            g_TaskSwitch = (task_id + 1) % 3;
        }
    }
};

static void InitLeds()
{
    LED_INIT(LED_RED, false);
    LED_INIT(LED_GREEN, false);
    LED_INIT(LED_BLUE, false);
}

void RunExample()
{
    using namespace stk;

    InitLeds();

    static Kernel<KERNEL_STATIC, 3, SwitchStrategyRoundRobin, PlatformDefault> kernel;
    static MyTask<ACCESS_PRIVILEGED> task1(0), task2(1), task3(2);

    kernel.Initialize();

    kernel.AddTask(&task1);
    kernel.AddTask(&task2);
    kernel.AddTask(&task3);

    kernel.Start(PERIODICITY_DEFAULT);

    assert(false);
    while (true);
}
```

---

## Test Coverage

* Platform-independent code: **100% unit test coverage**
* Platform-dependent code: tested under QEMU for each architecture

---

## Porting

Porting STK to a new platform is straightforward.

Platform-dependent files are located in:

```
stk/src/arch
stk/include/arch
```

[Contributions and patches](https://github.com/dmitrykos/stk) are welcome.

---

## License

STK is released under the **MIT License**.
You may freely use it in:

* commercial
* closed-source
* open-source
* academic

projects.

---

## Commercial Services

Contact: `stk@neutroncode.com`

* Dedicated license (warranty of title, perpetual usage rights)
* Integration and consulting
* Technical support

