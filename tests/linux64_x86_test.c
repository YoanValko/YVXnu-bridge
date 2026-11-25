// SPDX-License-Identifier: APSL-2.0
#include "../xnu-patches/include/linux64.h"

unsigned long linux64_hook;
unsigned long linux64_slide;
unsigned int linux64_vint[LINUX64_CPU_SLOTS];
unsigned int linux64_ipi_pending[LINUX64_CPU_SLOTS];
unsigned long linux64_dbg[LINUX64_NCOUNTERS];

static unsigned long check_call(void)
{
	return linux64_call2(LINUX64_CALL_PREEMPTED, 1, 2);
}

int main(void)
{
	return (int)(check_call() + linux64_cpu_slot());
}
