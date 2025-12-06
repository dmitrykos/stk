# C interface
C interface provides full access to C++ version of STK.

---

#### Example

```cpp
#include <stk_config.h>
#include <stk_c.h>

#define STACK_SIZE 256
uint32_t g_Stack[2][STACK_SIZE];

void task_func1(void *arg) {
    while (true) {
        stk_sleep_ms(1000);
    }
}

void task_func2(void *arg) {
    while (true) {
        stk_sleep_ms(500);
    }
}

int main(void) {
    stk_kernel_t *k = stk_kernel_create_static();

    stk_kernel_init(k, STK_PERIODICITY_DEFAULT);

    stk_task_t *t1 = stk_task_create_privileged(task_func1, (void *)0, g_Stack[0], STACK_SIZE);
    stk_task_t *t2 = stk_task_create_privileged(task_func2, (void *)1, g_Stack[1], STACK_SIZE);

    stk_kernel_add_task(k, t1);
    stk_kernel_add_task(k, t2);

    stk_kernel_start(k);

    STK_C_ASSERT(false);
}
```

Working example can be found in `build/example/project/eclipse/x86/blinky_c-mingw32`.