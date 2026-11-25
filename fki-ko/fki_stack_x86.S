// SPDX-License-Identifier: GPL-2.0-only
/* Run a callback on a Linux-owned stack while the caller is in FKI. */
	.text
	.globl fki_run_on_stack_x86
	.type fki_run_on_stack_x86,@function
fki_run_on_stack_x86:
	pushq	%rbp
	movq	%rsp, %rbp
	movq	%rsp, %r11
	movq	%rdx, %rsp
	andq	$-16, %rsp
	/* Keep rsp 16-byte aligned before the thunk call.  The callback then
	 * enters with the SysV-required rsp % 16 == 8. */
	subq	$16, %rsp
	movq	%r11, (%rsp)
	movq	%rdi, %r10
	movq	%rsi, %rdi
#ifdef USER_TEST
	call	*%r10
#else
	call	__x86_indirect_thunk_r10
#endif
	movq	(%rsp), %rsp
	popq	%rbp
#ifdef USER_TEST
	ret
#else
	jmp	__x86_return_thunk
#endif
	.size fki_run_on_stack_x86, .-fki_run_on_stack_x86
