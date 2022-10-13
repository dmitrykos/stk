/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STK_STRATEGY_RROBIN_H_
#define STK_STRATEGY_RROBIN_H_

#include "stk_common.h"

namespace stk {

class SwitchStrategyRoundRobin : public ITaskSwitchStrategy
{
public:
	IKernelTask *GetNext(IKernelTask *current)
	{
		return current->GetNext();
	}
};

} // namespace stk

#endif /* STK_STRATEGY_RROBIN_H_ */
