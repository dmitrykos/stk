# RISC-V architecture

---

STK expects the following MCU-specific register definitions:

* **MTIME**: ``STK_RISCV_CLINT_MTIME_ADDR``
* **MTIMECMP**: ``STK_RISCV_CLINT_MTIMECMP_ADDR``

**IMPORTANT:**
> RISC-V does not specify these registers at fixed addresses, check Datasheet or Technical Reference Manual of your MCU.

---

## RP2350 (Raspberry Pico 2)

You can find ready to use examples for RP2350 RISC-V (marked with ``_riscv``) in:
```
build/example/project/eclipse/rpi
```

``stk_config.h`` defines MTIME and MTIMECMP.

---

## ESP32-H2, ESP32-C6

* **ESP32-H2**: [Technical Reference Manual](https://documentation.espressif.com/esp32-h2_technical_reference_manual_en.pdf)
* **ESP32-C6**: [Technical Reference Manual](https://documentation.espressif.com/esp32-c6_technical_reference_manual_en.pdf)

Example of ``stk_config.h`` for ESP32 RISC-V MCU:

```cpp
#ifndef STK_CONFIG_H_
#define STK_CONFIG_H_

#include <risc-v/encoding.h>
#include <esp_attr.h>

// ESP32-H2/C6 RISC-V
#define _STK_ARCH_RISC_V

// MTIME, MTIMECMP
#define STK_RISCV_CLINT_BASE_ADDR     (0x2000000)
#define STK_RISCV_CLINT_MTIME_ADDR    ((volatile uint64_t *)(STK_RISCV_CLINT_BASE_ADDR + 0x1808)) // MTIME + MTIMEH
#define STK_RISCV_CLINT_MTIMECMP_ADDR ((volatile uint64_t *)(STK_RISCV_CLINT_BASE_ADDR + 0x1810)) // MTIMECMP + MTIMECMPH

// ISR handlers
#define _STK_SYSTICK_HANDLER m_timer_interrupt_handler
#define _STK_SVC_HANDLER     exception_handler

// Keep STK's ISR handlers in RAM
#define STK_RISCV_ISR_SECTION IRAM_ATTR

#endif /* STK_CONFIG_H_ */
```
