// SPDX-License-Identifier: APSL-2.0
	.text
	.globl world_test_entry
	.type world_test_entry,@function
world_test_entry:
	mov	$0x1111111111111111, %rbx
	mov	$0x2222222222222222, %rbp
	mov	$0x3333333333333333, %r12
	mov	$0x4444444444444444, %r13
	mov	$0x5555555555555555, %r14
	mov	$0x6666666666666666, %r15
	mov	%rsp, %rax
	and	$15, %rax
	cmp	$8, %rax
	jne	.Lbad_alignment
	lea	7(%rdi), %rax
	ret
.Lbad_alignment:
	mov	$-1, %rax
	ret
	.size world_test_entry, .-world_test_entry
