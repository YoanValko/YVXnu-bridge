// SPDX-License-Identifier: APSL-2.0
#include <stdio.h>
#include <string.h>
#include "../xnu-ko/world_x86.h"

struct xnu_world_x86 xnu_W_x86;
static int entered, left;

unsigned long fki_enter(const struct fki_pac_keys *foreign, struct fki_pac_keys *saved)
{
	(void)foreign;
	memset(saved, 0, sizeof(*saved));
	entered++;
	return 0xfeed;
}
void fki_leave(unsigned long cookie, const struct fki_pac_keys *saved)
{
	if (cookie == 0xfeed && saved)
		left++;
}

extern long world_test_entry(unsigned long arg);

int main(void)
{
	register unsigned long rbx asm("rbx") = 0xaaaa;
	register unsigned long rbp asm("rbp") = 0xbbbb;
	register unsigned long r12 asm("r12") = 0xcccc;
	register unsigned long r13 asm("r13") = 0xdddd;
	register unsigned long r14 asm("r14") = 0xeeee;
	register unsigned long r15 asm("r15") = 0xffff;
	long result = xnu_world_call_x86((unsigned long)world_test_entry, 35);
	asm volatile("" : "+r"(rbx), "+r"(rbp), "+r"(r12), "+r"(r13), "+r"(r14), "+r"(r15));
	int ok = result == 42 && entered == 1 && left == 1 &&
		rbx == 0xaaaa && r12 == 0xcccc &&
		r13 == 0xdddd && r14 == 0xeeee && r15 == 0xffff;
	if (!ok)
		printf("result=%ld entered=%d left=%d regs=%lx/%lx/%lx/%lx/%lx/%lx\n",
			result, entered, left, rbx, rbp, r12, r13, r14, r15);
	puts(ok ? "world_x86_test: PASS" : "world_x86_test: FAILED");
	return ok ? 0 : 1;
}
