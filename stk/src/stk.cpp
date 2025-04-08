/*
 * SuperTinyKernel: Minimalistic C++ thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2025 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#include "stk.h"
#include "arch/stk_arch_common.h"

// initialize IKernelService instance singleton
template <> stk::IKernelService *stk::Singleton<stk::IKernelService *>::m_instance = NULL;
