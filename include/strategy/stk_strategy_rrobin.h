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

/*! \class SwitchStrategyRoundRobin
    \brief Task switching strategy concrete implementation - Round-Robin.

    Tasks are given an equal amount of time.
*/
class SwitchStrategyRoundRobin : public ITaskSwitchStrategy
{
public:
	/*! \brief     Get next task.
		\param[in] current: Pointer to the current task.
		\return    Pointer to the next task.
	*/
	IKernelTask *GetNext(IKernelTask *current)
	{
	    return current->GetNext();
	}
};

} // namespace stk

#endif /* STK_STRATEGY_RROBIN_H_ */
