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

### Task Privilege Separation

Starting with ARM Cortex-M3 and all newer cores (M3/M4/M7/M33/M55 etc.) that implement the Armv7-M or Armv8-M architecture with the **Memory Protection Unit (MPU)**, STK supports explicit privilege separation between tasks.

| Access Mode         | Privileged (`ACCESS_PRIVILEGED`)                               | Unprivileged (`ACCESS_USER`)                                       |
|---------------------|----------------------------------------------------------------|--------------------------------------------------------------------|
| CPU privilege level | Runs in **Privileged Thread Mode**                             | Runs in **Unprivileged Thread Mode**                               |
| Direct peripheral access | Allowed (normal register/bit-band access)                | Blocked by the hardware (BusFault on any peripheral access)        |
| Ability to call SVC / trigger PendSV | Yes                                                  | No (but STK services allow Sleep, Delay, Yield, CS, ...)           |
| Ability to execute privileged instructions (CPS, MRS/MSR for control regs, etc.) | Yes | No                                                                 |
| Typical use case    | Drivers, hardware abstraction, critical infrastructure code  | Application logic, protocol parsers, third-party or untrusted code |

#### Why this matters

Modern embedded systems increasingly process **untrusted or complex data** (network packets, USB descriptors, sensor protocols, firmware updates, etc.). A single bug in data parsing code can corrupt peripheral registers, disable interrupts, or even brick the device.

By marking tasks that parse potentially attacker-controlled data as `ACCESS_USER`, you get **hardware-enforced isolation**:

- An erroneous or malicious write to a peripheral register immediately triggers a **hard BusFault** instead of silently corrupting hardware state.
- Only explicitly trusted tasks (marked `ACCESS_PRIVILEGED`) are allowed to touch GPIO, UART, SPI, DMA, timers, etc.
- The kernel itself and all STK services remain fully functional for unprivileged tasks (sleep, yield, critical sections, TLS, etc.).

#### Example

```cpp
// Trusted driver task – needs direct hardware access
class DriverTask : public stk::Task<256, ACCESS_PRIVILEGED> { ... };

// Application task that parses USB or network data – runs unprivileged
class ParserTask : public stk::Task<512, ACCESS_USER> { ... };
```

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

## Quick Start (1 minute)

### 1. Clone repository
```bash
git clone https://github.com/dmitrykos/stk.git
cd stk
```

### 2. Build example for x86 development mode

with Visual Studio 2019:

```bash
cd build/example/project/msvc
```
* `blinky` – emulates toggling of Red, Green, Blue LEDs
* `critical_section` – demonstrates support for Critical Section synchronization primitive
* `tls` - demonstrates the use of Thread-local storage (TLS)

with Eclipse CDT:
```bash
cd build/example/project/eclipse
```
* `blinky-mingw32` – emulates toggling of Red, Green, Blue LEDs

To import project into Eclipse workspace:

```
File → Import... → Existing Projects into Workspace →
Select root directory → Browse... → build/example/project/eclipse/blinky-mingw32
```

### 3. Run on hardware
* Import the STM32, RPI (Raspberry Pico) or NXP example in Eclipse CDT IDE or MCUXpresso IDE
* Build and flash your target MCU

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
File → Import... → Existing Projects into Workspace →
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

Below example toggles RGB LEDs on a development board. Each LED is controlled by its own thread, switching at 1s intervals:

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

## Adding STK to your project


### Add using Git & CMake:


#### 1. Add STK to your project using Git & CMake
You can include STK in your project using `git submodule` or by copying the source into a `libs/` or `third_party/` folder.

```bash
# Example: using git submodule
cd your-project
git submodule add https://github.com/dmitrykos/stk.git libs/stk_scheduler
git submodule update --init
```

#### 2. Modify your `CMakeLists.txt`
Add the STK directory and link against STK:

```cmake
# In your project CMakeLists.txt

# 1. Add STK as a subdirectory
add_subdirectory(libs/stk_scheduler/stk)

# 2. Link STK into your executable or firmware target
target_link_libraries(your_firmware_target PUBLIC stk)

# 3. Include STK headers
target_include_directories(your_firmware_target
    PUBLIC
    ${CMAKE_SOURCE_DIR}/libs/stk_scheduler/stk/include
)
```

#### 3. Build
Run your normal build procedure. STK will now be compiled and linked with your project.

#### 4. Initialize STK in your code

```cpp
#include "stk.h"
// ...
static Kernel<KERNEL_STATIC, 3, SwitchStrategyRoundRobin, PlatformDefault> kernel;
// add tasks, start scheduling …
```

#### 5. Testing & Simulation
- Use STK’s x86 development mode for rapid development
- Deploy to MCU when ready


### Alternative Method: Copy STK directly into project files:


If you prefer not to use `git submodule` or external dependencies, you can integrate STK by simply copying its source files.

This method is suitable for:
- vendor-delivered projects (MCUXpresso, STM32CubeIDE, Keil, IAR)
- closed-source or isolated environments
- projects without CMake or external dependency management

#### 1. Copy STK folders
From the root of the STK repository, copy:

```
stk/
```

into your project's source tree, for example:

```
your_project/
  src/
  drivers/
  libs/
    stk/   ← copied here
```

#### 2. Add STK include paths
Add the following include path to your project configuration:

```
your_project/libs/stk/include
```

In CMake:
```cmake
target_include_directories(your_firmware_target
    PUBLIC
    ${CMAKE_SOURCE_DIR}/libs/stk/include
)
```

In GCC/Makefile:
```make
-Ilibs/stk/include
```

#### 4. Create stk_config.h and add it to includes

For ARM Cortex-M4 project:

```cpp
#ifndef STK_CONFIG_H_
#define STK_CONFIG_H_

#include "cmsis_device.h"
#include "core_cm4.h"

// Undefine if MCU is Arm Cortex-M4
#define _STK_ARCH_ARM_CORTEX_M

#ifdef _STK_ARCH_ARM_CORTEX_M
    // Redefine if SysTick handler name is different from SysTick_Handler
    //#define _STK_SYSTICK_HANDLER SysTick_Handler

    // Redefine if PendSv handler name is different from PendSV_Handler
    //#define _STK_PENDSV_HANDLER PendSV_Handler

    // Redefine if SVC handler name is different from SVC_Handler
    //#define _STK_SVC_HANDLER SVC_Handler
#endif

#endif /* STK_CONFIG_H_ */
```

#### 5. Add STK source files to build
You must compile STK core sources from:
```
stk/src
```

Minimum required sources:
```
stk/src/stk.cpp
stk/src/arch/<your-platform>/...
```

Example (GCC, ARM Cortex-M MCU):
```make
SRCS += \
  libs/stk/src/stk.cpp \
  libs/stk/src/arch/arm/cortex-m/stk_arch_arm-cortex-m.cpp
```

#### 4. Build
Build your project normally — STK will now be compiled together with it.

#### Pros
✅ Simplest integration
✅ No dependency management required
✅ Works offline

#### Cons
⚠ Manual updates required when STK changes
⚠ Easy to accidentally exclude platform files

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

