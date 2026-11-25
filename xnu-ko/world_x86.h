// SPDX-License-Identifier: APSL-2.0
/* First x86-64 world-switch ABI.  This is deliberately separate from the
 * ARM64 layout in world.h: no ARM system registers or PAC state exist here. */
#pragma once

#include "../fki-ko/fki.h"

struct xnu_world_x86 {
	unsigned long rbx, rbp, r12, r13, r14, r15, rsp;
	unsigned long irq_cookie;
	struct fki_pac_keys linux_keys;
	struct fki_pac_keys xnu_keys;
	unsigned long linux_cr3;
	unsigned long xnu_cr3;
	unsigned long linux_fsbase;
	unsigned long xnu_fsbase;
	unsigned long linux_gsbase;
	unsigned long xnu_gsbase;
};

_Static_assert(__builtin_offsetof(struct xnu_world_x86, irq_cookie) == 56,
		"x86 world layout");
_Static_assert(__builtin_offsetof(struct xnu_world_x86, linux_keys) == 64,
		"x86 world layout");
_Static_assert(__builtin_offsetof(struct xnu_world_x86, xnu_keys) == 152,
		"x86 world layout");

extern struct xnu_world_x86 xnu_W_x86;

/* Runs fn(arg) with the host ABI saved.  CR3/FS/GS switching is intentionally
 * a later step: these fields are reserved so the ABI does not change. */
long xnu_world_call_x86(unsigned long fn, unsigned long arg);
