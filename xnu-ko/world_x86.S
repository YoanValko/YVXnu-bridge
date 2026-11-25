/* SPDX-License-Identifier: APSL-2.0 */
/* Minimal AMD64 foreign-world entry.  It is safe for the current bootstrap:
 * it saves the SysV callee-saved registers and uses the x86 FKI enter/leave
 * pair.  Address-space and exception switching are deliberately not hidden
 * here; those belong in the next x86 machine-layer step. */

	.text
	.globl	xnu_world_call_x86
	.type	xnu_world_call_x86,@function
xnu_world_call_x86:
	mov	%rsp, %r10
	push	%rbp
	mov	%rsp, %rbp
	lea	xnu_W_x86(%rip), %r11
	mov	%rbx, 0(%r11)
	mov	%rbp, 8(%r11)
	mov	%r12, 16(%r11)
	mov	%r13, 24(%r11)
	mov	%r14, 32(%r11)
	mov	%r15, 40(%r11)
	mov	%r10, 48(%r11)
	/* The rbp push above changes the SysV entry stack (+8) to 16-byte
	 * alignment before the calls below. */
	mov	%rdi, %r12
	mov	%rsi, %r13
	lea	152(%r11), %rdi
	lea	64(%r11), %rsi
	call	fki_enter
	lea	xnu_W_x86(%rip), %r11
	mov	%rax, 56(%r11)
	mov	%r13, %rdi
	/* Use the kernel's exported retpoline thunk for the foreign entry. */
#ifdef USER_TEST
	call	*%r12
#else
	call	__x86_indirect_thunk_r12
#endif
	mov	%rax, %r14
	lea	xnu_W_x86(%rip), %r11
	mov	56(%r11), %rdi
	lea	64(%r11), %rsi
	call	fki_leave
	mov	%r14, %rax
	lea	xnu_W_x86(%rip), %r11
	mov	0(%r11), %rbx
	mov	8(%r11), %rbp
	mov	16(%r11), %r12
	mov	24(%r11), %r13
	mov	32(%r11), %r14
	mov	40(%r11), %r15
	mov	48(%r11), %rsp
#ifdef USER_TEST
	ret
#else
	jmp	__x86_return_thunk
#endif
	.size	xnu_world_call_x86, .-xnu_world_call_x86
