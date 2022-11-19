/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
 * License: MIT License, see LICENSE for a full text.
 */

#include "stk.h"

using namespace stk;

// initialize IKernelService instance singleton
template <> IKernelService *Singleton<IKernelService *>::m_instance = NULL;
