// SPDX-License-Identifier: GPL-2.0-only
#include <stdint.h>
#include <stdio.h>

extern void fki_run_on_stack_x86(void (*fn)(void *), void *arg, void *top);
static uintptr_t callback_rsp;
static uintptr_t callback_arg;

static void callback(void *arg)
{
	asm volatile("mov %%rsp, %0" : "=r"(callback_rsp));
	callback_arg = (uintptr_t)arg;
}

int main(void)
{
	unsigned char stack[8192] __attribute__((aligned(16)));
	fki_run_on_stack_x86(callback, (void *)0x1234, stack + sizeof(stack));
	int ok = callback_arg == 0x1234 && (callback_rsp & 15) == 8;
	puts(ok ? "stack_x86_test: PASS" : "stack_x86_test: FAILED");
	return ok ? 0 : 1;
}
