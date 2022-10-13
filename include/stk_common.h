/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STK_COMMON_H_
#define STK_COMMON_H_

#include "stk_defs.h"

namespace stk {

typedef void (*RunFuncT) (void *userData);

enum EAccessMode
{
	ACCESS_USER = 0, //!< Unprivileged access mode (access to some hardware is restricted, see CPU manual for details).
	ACCESS_PRIVILEGED //!< Privileged access mode (access to hardware is fully unrestricted).
};

struct Stack
{
	size_t SP; // SP register
};

class ITask
{
public:
	virtual RunFuncT GetFunc() = 0;
	virtual void *GetFuncUserData() = 0;
	virtual size_t *GetStack() = 0;
	virtual uint32_t GetStackSize() const = 0;
	virtual EAccessMode GetAccessMode() const = 0;
};

template <uint32_t _StackSize, EAccessMode _AccessMode>
class UserTask : public ITask
{
public:
	size_t *GetStack() { return Stack; }
	uint32_t GetStackSize() const { return _StackSize; }
	EAccessMode GetAccessMode() const { return _AccessMode; }

private:
	__stk_aligned(8) size_t Stack[_StackSize];
};

class IKernelTask
{
public:
	virtual IKernelTask *GetNext() = 0;
	virtual ITask *GetUserTask() = 0;
	virtual Stack *GetUserStack() = 0;
};

class IPlatform
{
public:
	class EventHandler
	{
	public:
		virtual void OnStart() = 0;
		virtual void OnSysTick(Stack **idle, Stack **active) = 0;
	};

	virtual void Start(IPlatform::EventHandler *eventHandler, uint32_t resolution_us, IKernelTask *firstTask) = 0;
	virtual bool InitStack(Stack *stack, ITask *userTask) = 0;
	virtual void SwitchContext() = 0;
	virtual int32_t GetSysTickResolution() const = 0;
	virtual void SetAccessMode(EAccessMode mode) = 0;
};

class ITaskSwitchStrategy
{
public:
	virtual IKernelTask *GetNext(IKernelTask *current) = 0;
};

class IKernel
{
public:
	virtual void Initialize(IPlatform *platformDriver, ITaskSwitchStrategy *switchStrategy) = 0;
	virtual void AddTask(ITask *userTask) = 0;
	virtual void Start(uint32_t resolutionMs) = 0;
};

class IKernelService
{
public:
	virtual int64_t GetTicks() const = 0;
	virtual int32_t GetTicksResolution() const = 0;

	int64_t GetDeadlineTicks(int64_t deadline_ms) const
	{
		return GetTicks() + deadline_ms * GetTicksResolution() / 1000;
	}

	void Delay(uint32_t delay_ms)
	{
		int64_t ticks = GetDeadlineTicks(delay_ms);
		while (GetTicks() < ticks);
	}
};

} // namespace stk

#endif /* STK_COMMON_H_ */
