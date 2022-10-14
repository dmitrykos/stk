# SuperTinyKernel (STK)
Minimalistic thread scheduling kernel for the Embedded systems.


## About:

STK tends to me as **minimal** as possible to be able to provide a multi-threading 
capability for your Embedded system without any attempt to abstract operations
with peripherals. It does not pretent to be a Real-Time OS (**RTOS**) but instead
it gives possibility to add multi-threading into a bare-metal project with
a very little effort.

STK is developed in C++ and follows an Object-Oriented Design principles while 
at the same time does not pollute namespace with exceeding declarations, nor 
using fancy new C++ features. It just tries to be very friendly to C developers ;)

## Supports:
* Arm Cortex-M0
* Arm Cortex-M3
* Arm Cortex-M4
* Arm Cortex-M7

## Requires:
* CMSIS
* Vendor BSP (NXP, STM, ...)

## Example:

Here is an example to toggle Red, Green, Blue LEDs of the FRM-K66 development
board (NXP K66 MCU) where thread is handling its own LED, e.g. there are 3 threads
in total which are switching LEDs with 1 second periodicity.

```cpp
#include <stk.h>
#include "board.h"

static volatile uint8_t g_TaskSwitch = 0;

template <stk::EAccessMode _AccessMode>
class Task : public stk::UserTask<256, _AccessMode>
{
    uint8_t m_taskId;

public:
    Task(uint8_t taskId) : m_taskId(taskId)
    { }

    stk::RunFuncT GetFunc() { return &Run; }
    void *GetFuncUserData() { return this; }
    static void Run(void *userData)
    {
        Task *_this = (Task *)userData;
        _this->RunInner();
    }

protected:
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
                LED_RED_ON();
                LED_GREEN_OFF();
                LED_BLUE_OFF();
                break;
            case 1:
                LED_RED_OFF();
                LED_GREEN_ON();
                LED_BLUE_OFF();
                break;
            case 2:
                LED_RED_OFF();
                LED_GREEN_OFF();
                LED_BLUE_ON();
                break;
            }

            stk::g_Kernel->Delay(1000);

            g_TaskSwitch = task_id + 1;
            if (g_TaskSwitch > 2)
                g_TaskSwitch = 0;
        }
    }
};

static void InitLeds()
{
    LED_RED_INIT(LOGIC_LED_OFF);
    LED_GREEN_INIT(LOGIC_LED_OFF);
    LED_BLUE_INIT(LOGIC_LED_OFF);
}

void RunExample()
{
    using namespace stk;

    InitLeds();

    Kernel<10> kernel;
    PlatformArmCortexM platform;
    SwitchStrategyRoundRobin tsstrategy;

    Task<ACCESS_PRIVILEGED> task1(0);
    Task<ACCESS_USER> task2(1);
    Task<ACCESS_USER> task3(2);

    kernel.Initialize(&platform, &tsstrategy);

    kernel.AddTask(&task1);
    kernel.AddTask(&task2);
    kernel.AddTask(&task3);

    kernel.Start(1000);

    assert(false);
    while (true);
}
```

## Test boards:
* Arm Cortex-M0
* Arm Cortex-M3
* Arm Cortex-M4
  - [NXP FRDM-K66F](http://www.google.com/search?q=FRDM-K66F)
  - [STM STM32F4DISCOVERY](http://www.google.com/search?q=STM32F4DISCOVERY)
* Arm Cortex-M7
  - [NXP MIMXRT1050 EVKB](http://www.google.com/search?q=MIMXRT1050-EVKB)

## Porting:

You are welcome to port STK to a new platform and offer a patch. The platform
dependent files are located in: ```/src/arch``` and ```/include/arch``` folders.