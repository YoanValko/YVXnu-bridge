// SPDX-License-Identifier: APSL-2.0
#include "world_x86.h"

/* The x86 bootstrap keeps one world until per-CPU CR3/GS state is added. */
struct xnu_world_x86 xnu_W_x86;
