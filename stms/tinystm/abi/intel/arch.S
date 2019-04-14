#ifndef __linux__
/* Linux specific, Windows uses a different calling convention. */
/* TODO probably solaris is fine with it. */
# error This file is linux specific.
#endif

	.text

#if defined(__x86_64__)
	.align 4
	.globl	_ITM_beginTransaction
	.type	_ITM_beginTransaction, @function

_ITM_beginTransaction:
	.cfi_startproc
/* Parameters (in order) is in rdi, rsi, rdx, rcx, r8, r9 */
/* Temporary registers is r10, r11 (not saved) */
/* To be saved are rbx, rsp, rbp, r12, r13, r14, r15 */
	leaq	8(%rsp), %rax    /* Save stack pointer */
	subq	$56, %rsp
	.cfi_def_cfa_offset 64
	movq	%rax, (%rsp)
	movq	%rbx, 8(%rsp)
	movq	%rbp, 16(%rsp)
	movq	%r12, 24(%rsp)
	movq	%r13, 32(%rsp)
	movq	%r14, 40(%rsp)
	movq	%r15, 48(%rsp)
	movq	%rsp, %rdx
	call	GTM_begin_transaction
	addq	$56, %rsp
	.cfi_def_cfa_offset 8
	ret
	.cfi_endproc
	.size	_ITM_beginTransaction, .-_ITM_beginTransaction

	.align 4
	.globl	_ITM_siglongjmp
	.type	_ITM_siglongjmp, @function
	.hidden	_ITM_siglongjmp

_ITM_siglongjmp:
	.cfi_startproc
	movq	(%rsi), %rcx
	movq	8(%rsi), %rbx
	movq	16(%rsi), %rbp
	movq	24(%rsi), %r12
	movq	32(%rsi), %r13
	movq	40(%rsi), %r14
	movq	48(%rsi), %r15
	movl	%edi, %eax
	.cfi_def_cfa %rsi, 0
	.cfi_offset %rip, 56
	.cfi_register %rsp, %rcx
	movq	%rcx, %rsp
	/* a_restoreLiveVariables = 0x08 */
	orq 	$8, %rax
	jmp	*56(%rsi)
	.cfi_endproc
	.size	_ITM_siglongjmp, .-_ITM_siglongjmp

#elif defined(__i386__)
	.align 4
	.globl	_ITM_beginTransaction
	.type	_ITM_beginTransaction, @function
_ITM_beginTransaction:
	.cfi_startproc
	/* eax: tx, edx: attr, stack: __src */
	leal	4(%esp), %ecx
	subl	$28, %esp
	.cfi_def_cfa_offset 32
	movl	%ecx, 8(%esp)
	movl	%ebx, 12(%esp)
	movl	%esi, 16(%esp)
	movl	%edi, 20(%esp)
	movl	%ebp, 24(%esp)
	leal	8(%esp), %ecx
	push	%ecx
	/* GTM_begin_transaction(eax: tx, edx: attr, stack: jmp_buf) */
	call	GTM_begin_transaction
	addl	$32, %esp
	.cfi_def_cfa_offset 4
	ret
	.cfi_endproc
	.size	_ITM_beginTransaction, .-_ITM_beginTransaction


	.align 4
	.globl	_ITM_siglongjmp
	.type	_ITM_siglongjmp, @function
	.hidden	_ITM_siglongjmp
_ITM_siglongjmp:
	.cfi_startproc
	movl	(%edx), %ecx
	movl	4(%edx), %ebx
	movl	8(%edx), %esi
	movl	12(%edx), %edi
	movl	16(%edx), %ebp
	.cfi_def_cfa %edx, 0
	.cfi_offset %eip, 20
	.cfi_register %esp, %ecx
	movl	%ecx, %esp
	/* TODO check correctness of this behaviour, a_restoreLiveVariables = 0x08 */
	orl 	$8, %eax
	jmp	*20(%edx)
	.cfi_endproc
	.size	_ITM_siglongjmp, .-_ITM_siglongjmp

#else
# error Unsupported architecture
#endif

#ifdef __linux__
.section .note.GNU-stack, "", @progbits
#endif
