# SuperTinyKernel (STK)
Minimalistic thread scheduling kernel for the Embedded systems.


## About
STK tends to me as **minimal** as possible to be able to provide a multi-threading 
capability for your Embedded system without any attempt to abstract operations
with peripherals. It does not pretent to be a Real-Time OS (**RTOS**) but instead
it gives possibility to add multi-threading into a bare-metal project with
a very little effort.

STK is developed in C++ and follows an Object-Oriented Design principles while 
at the same time does not pollute namespace with exceeding declarations, nor 
using fancy new C++ features. It just tries to be very friendly to C developers ;)

## Features
STK supports soft real-time (default) and hard-real time (HRT) modes of operation. 
It supports infinite looping (```KERNEL_STATIC```), finite (```KERNEL_DYNAMIC```) 
and periodic (HRT mode - ```KERNEL_HRT```) tasks.

STK intercepts main process program flow if it is in ```KERNEL_STATIC``` mode but it can
also return into the main process when all tasks exited in case of ```KERNEL_DYNAMIC``` 
mode.

HRT mode allows to run periodic tasks which can be finite or infinite depending on whether
```KERNEL_STATIC``` or ```KERNEL_DYNAMIC``` mode is used in addition to the ```KERNEL_HRT```.
HRT tasks are checked for a deadline miss by STK automatically therefore it guarantees 
a ***fully deterministic behavior*** of the application.

STK's run-time performance is comparable to other well known C-based thread scedulers but its code base is much slimmer and therefore easier to test, maintain and advance.

## Hardware support

### MCU
* Arm Cortex-M0
* Arm Cortex-M3
* Arm Cortex-M4
* Arm Cortex-M7
### Floating point
* Soft, Hard

## Requires
* CMSIS
* Vendor BSP (NXP, STM, ...)

## Example
Here is an example to toggle Red, Green, Blue LEDs of the NXP FRM-K66F or 
STM STM32F4DISCOVERY development boards hosting Arm Cortex-M4F CPU where thread is 
handling its own LED, e.g. there are 3 threads in total which are switching LEDs 
with 1 second periodicity.

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

        float count = 0;
        uint64_t count_skip = 0;

        while (true)
        {
            if (g_TaskSwitch != task_id)
            {
                ++count_skip;
                continue;
            }

            ++count;

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

            g_KernelService->Sleep(delay_ms);

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

## Test boards
* Arm Cortex-M0
  - [STM STM32F0DISCOVERY](https://www.st.com/en/evaluation-tools/stm32f0discovery.html)
* Arm Cortex-M3
  - [STM NUCLEO-F103RB](https://www.st.com/en/evaluation-tools/nucleo-f103rb.html)
* Arm Cortex-M4 (Cortex-M4F)
  - [NXP FRDM-K66F](http://www.google.com/search?q=FRDM-K66F)
  - [STM STM32F4DISCOVERY](http://www.google.com/search?q=STM32F4DISCOVERY)
* Arm Cortex-M7
  - [NXP MIMXRT1050 EVKB](http://www.google.com/search?q=MIMXRT1050-EVKB)

## Build
It is fairly easy to build and run examples without even having embedded hardware on your hands. You will need these tools to run examples on your PC:

* [Eclipse Embedded CDT (C/C++ Development Tools)](https://projects.eclipse.org/projects/iot.embed-cdt)

**Arm platform:**
* Compiler: [The xPack GNU Arm Embedded GCC](https://xpack.github.io/dev-tools/arm-none-eabi-gcc)
* Emulator: [The xPack QEMU Arm](https://xpack.github.io/dev-tools/qemu-arm)

In case of NXP MCU you will only need [MCUXpresso IDE](https://www.nxp.com/design/software/development-software/mcuxpresso-software-and-tools-/mcuxpresso-integrated-development-environment-ide:MCUXpresso-IDE) which is comes with bundled GCC compler.
  
**RISC-V platform:**
* Compiler: [The xPack GNU RISC-V Embedded GCC](https://xpack.github.io/dev-tools/riscv-none-elf-gcc)
* Emulator: [The xPack QEMU RISC-V](https://xpack.github.io/dev-tools/qemu-riscv)

If you are working with only Arm platform then you only need Arm-related tools.

Generic eclipse examples are located in the ```build/example/project/eclipse``` folder and sorted by platform:

* **stm** - Arm platform, examples based on STM32 microcontrollers and can be executed on QEMU virtual machine or directly on hardware if you have corresponding board.
* **risc-v** - RISC-V platform, examples based on QEMU virtual machine.
* **x86** - x86 platform, examples are executed on x86 CPU.

Additionally, examples for NXP MCUXpresso IDE are provided in ```build/example/project/nxp-mcuxpresso``` folder. These examples are compatible with NXP Kinetis® K66, Kinetis® K26 and NXP i.MX RT1050 MCUs and can be executed directly on corresponding evaluation boards.

## Porting
You are welcome to port STK to a new platform and offer a patch. The platform
dependent files are located in: ```stk/src/arch``` and ```stk/include/arch``` folders.