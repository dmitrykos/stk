# STK Synchronization Module (`stk::sync`)

**STK Synchronization Module** provides a set of high-performance synchronization primitives designed specifically for embedded systems. These classes facilitate task communication, resource protection, and signaling. Waiting operations are supported directly by the kernel.

## Features

- **Zero Dynamic Allocation**: Objects are allocated statically or on stack. The C API uses opaque memory containers (`stk_xxx_mem_t`) to guarantee alignment and size without heap usage.
- **Direct Resource Handover**: Resources are immediately assigned to the head of the wait list upon release. If the newly readied task is the highest-priority, an immediate context switch occurs.
- **Low-Power Optimization**: Blocking `Wait` operations remove tasks from the ready list, allowing the CPU to enter low-power sleep (e.g., `WFI`).
- **Strict FIFO Ordering**: Wait operations are processed chronologically. The kernel wakes the longest-waiting task first, ensuring absolute fairness.
- **Non-blocking Polling**: Supports `TryWait` and `TryLock` for checking status without yielding the CPU, ideal for zero-latency performance loops.
- **Nesting Support**: Both `sync::Mutex` and `sync::SpinLock` support recursive acquisition from the same thread.
- **C compatible**: While implemented in C++, a comprehensive C API is provided to allow these primitives to be used in pure C projects.

---

## Primitives

### 1. Mutex (`sync::Mutex`)
A recursive mutual exclusion primitive used to protect shared resources.
- **Features**: Supports `Lock`, `TryLock`, `Unlock`, and `TimedLock`.
- **Low-Power Aware**: Waiting tasks are suspended by the kernel.

### 2. Spinlock (`sync::SpinLock`)
A high-performance recursive spinlock for very short critical sections.
- **Configurable Spin Count**: Busy-waits for N iterations before calling `Yield()`, preventing total system stall while waiting for the lock.
- **Low Latency**: Bypasses the kernel wait-list logic for the "fast path" acquisition.
- **Low-Power Aware**: Waiting tasks will be switched out if busy-waiting exceeds a threshold.

### 3. Condition Variable (`sync::ConditionVariable`)
Used in conjunction with a Mutex to wait for specific application states.
- **Features**: `Wait`, `TryWait`, `NotifyOne`, and `NotifyAll`.
- **Real-time**: Releases mutex and suspends task atomically, ensuring no "lost wake-up" signals.
- **Low-Power Aware**: Waiting tasks are suspended by the kernel.

### 4. Event (`sync::Event`)
A binary signaling primitive supporting Auto-reset and Manual-reset modes.
- **Manual Reset**: Remains signaled until explicitly reset.
- **Auto Reset**: Resets automatically after waking a single waiting task.
- **Pulse**: Wakes waiting tasks and immediately resets the event (similar to Win32 API).
- **Low-Power Aware**: Waiting tasks are suspended by the kernel.

### 5. Semaphore (`sync::Semaphore`)
A counting signaling primitive used for resource tracking or producer-consumer patterns.
- **Direct Handover**: When semaphore is signaled, kernel immediately transfers the resource to the first waiting task (FIFO ordering).
- **Low-Power Aware**: Waiting tasks are suspended by the kernel.

### 6. Pipe (`sync::Pipe<T, Capacity>`)
A thread-safe, lock-free (for single producer/consumer), or mutex-protected FIFO buffer.
- **Template-based**: Supports any data type.
- **Bulk Operations**: Optimized `ReadBulk` and `WriteBulk` using `memcpy` for high-throughput data like audio samples.
- **Low-Power Aware**: Waiting tasks are suspended by the kernel.

---

## ISR Safety

STK primitives follow strict rules for **Interrupt Service Routine (ISR)** contexts to ensure system stability.

The following operations are ISR-safe:
* **sync::Event**: `Set()`, `Pulse()`, `Reset()`, `TryWait()`
* **sync::Semaphore**: `Signal()`
* **sync::ConditionVariable**: `NotifyOne()`, `NotifyAll()`, `Wait(NO_WAIT)`

---

## Examples

* C++: [sync](https://github.com/dmitrykos/stk/tree/main/build/example/sync/example.cpp)
* C: [sync_c](https://github.com/dmitrykos/stk/tree/main/build/example/sync_c/example.c)

### Eclipse projects:
* STM32F407G-DISC1, C++: [sync-stm32f407g-disc1](https://github.com/dmitrykos/stk/tree/main/build/example/project/eclipse/stm/sync-stm32f407g-disc1)
* Raspberry RP2350 ARM Cortex-M (dual or single core cases), C++: [sync_c-rp2350w](https://github.com/dmitrykos/stk/tree/main/build/example/project/eclipse/rpi/sync-rp2350w)
* Raspberry RP2350 ARM Cortex-M (dual or single core cases), C: [sync-rp2350w](https://github.com/dmitrykos/stk/tree/main/build/example/project/eclipse/rpi/sync_c-rp2350w)
* Raspberry RP2350 RISC-V (dual or single core cases), C++: [sync-rp2350w-riscv](https://github.com/dmitrykos/stk/tree/main/build/example/project/eclipse/rpi/sync-rp2350w)

### C++ Usage Demo

```cpp
#include <sync/stk_sync.h>

stk::sync::Mutex g_Mutex;
stk::sync::Event g_DataReady(false); // auto-reset

void TaskA() {
    g_Mutex.Lock();
    // ... access shared resource ...
    g_Mutex.Unlock();
    
    // signal TaskB
    g_DataReady.Set();
}

void TaskB() {
    // wait up to 1000ms for signaling
    if (g_DataReady.Wait(1000)) {
        // ... process data ...
    }
}
```

### Equivalent C Usage Demo

```c
#include <stk_c.h>

static stk_mutex_mem_t g_MtxMem;
static stk_event_mem_t g_EvtMem;
static stk_mutex_t *g_Mutex;
static stk_event_t *g_DataReady;

void Init() {
    g_Mutex = stk_mutex_create(&g_MtxMem, sizeof(g_MtxMem));
    g_DataReady = stk_event_create(&g_EvtMem, sizeof(g_EvtMem), false); // auto-reset
}

void TaskA(void *arg) {
    stk_mutex_lock(g_Mutex);    
    // ... access shared resource ...    
    stk_mutex_unlock(g_Mutex);
    
    // signal TaskB
    stk_event_set(g_DataReady);
}

void TaskB(void *arg) {
    while (true) {
        // wait up to 1000ms for signaling
        if (stk_event_wait(g_DataReady, 1000)) {
            // ... process data ...
        }
    }
}
```