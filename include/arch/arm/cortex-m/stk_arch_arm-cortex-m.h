/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STK_ARCH_ARM_CORTEX_M_H_
#define STK_ARCH_ARM_CORTEX_M_H_

#include "stk_common.h"

namespace stk {

/*! \class PlatformArmCortexM
    \brief Concrete implementation of the platform driver for the Arm Cortex-M0, M3, M4, M7 processors.
*/
class PlatformArmCortexM : public IPlatform
{
public:
    void Start(IEventHandler *event_handler, uint32_t resolution_us, IKernelTask *firstTask);
    bool InitStack(Stack *stack, ITask *userTask);
    void SwitchContext();
    int32_t GetTickResolution() const;
    void SetAccessMode(EAccessMode mode);
};

} // namespace stk

#endif /* STK_ARCH_ARM_CORTEX_M_H_ */
