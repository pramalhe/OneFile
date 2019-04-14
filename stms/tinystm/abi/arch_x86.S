/* Copyright (C) 2008, 2009 Free Software Foundation, Inc.
   Contributed by Richard Henderson <rth@redhat.com>.

   This file is part of the GNU Transactional Memory Library (libitm).

   Libitm is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   Libitm is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
   more details.

   Under Section 7 of GPL version 3, you are granted additional
   permissions described in the GCC Runtime Library Exception, version
   3.1, as published by the Free Software Foundation.

   You should have received a copy of the GNU General Public License and
   a copy of the GCC Runtime Library Exception along with this program;
   see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
   <http://www.gnu.org/licenses/>.  */
/* Modify for TinySTM and Intel STM Compiler */

/* Linux specific, Windows uses a different calling convention. */
#ifndef __linux__
#error This file is linux specific.
#endif



	.text
	.p2align 4
	.globl	_ITM_beginTransaction
	.type	_ITM_beginTransaction, @function

_ITM_beginTransaction:
	.cfi_startproc
#ifdef __LP64__
/* Paramters (in order) is in rdi, rsi, rdx, rcx, r8, r9 */
/* Temporary registers is r10, r11 (not saved) */
/* To be saved are rbx, rsp, rbp, r12, r13, r14, r15 */
/* push register can be also used */
	leaq	8(%rsp), %rax     /* Save stack pointer */ 
	movq	(%rsp), %r8       /* Save return address */
#if defined(EXPLICIT_TX_PARAMETER)
/* NOTE we already have transaction descriptor, we copy directly to tx->env (first field of stm_tx_t) and not on the stack */
	movq	%rax, (%rdi)      /* rax is the original stack pointer */
	movq	%r8, 8(%rdi)      /* r8 is the return address */
	movq	%rbx, 16(%rdi)    /* saves all registers needed */
	movq	%rbp, 24(%rdi)
	movq	%r12, 32(%rdi)
	movq	%r13, 40(%rdi)
	movq	%r14, 48(%rdi)
	movq	%r15, 56(%rdi)
        movq    %rdi, %rdx    /* set 3rd parameter, 2nd and 1st is untouched */
	call	GTM_begin_transaction
#else
	subq	$72, %rsp         /* Allocate space from the stack */
	.cfi_def_cfa_offset 80    /* Debug info */
	movq	%rax, (%rsp)      /* rax is the original stack pointer */
	movq	%r8, 8(%rsp)      /* r8 is the return address */
	movq	%rbx, 16(%rsp)    /* saves all registers needed */
	movq	%rbp, 24(%rsp)
	movq	%r12, 32(%rsp)
	movq	%r13, 40(%rsp)
	movq	%r14, 48(%rsp)
	movq	%r15, 56(%rsp)
	movq	%rsp, %rsi    /* set 2nd paramter (pointer to the stack where registers are saved), 1st is untouched (rdi) */
	call	GTM_begin_transaction
	addq	$72, %rsp         /* Remove allocated space from the stack */
#endif
	.cfi_def_cfa_offset 8
	ret
#else /* !LP64  */
/* NOTE attribute regparm(2) has to be set for all itm function and push 3rd argument on stack with icc */
/* fastcall convention argument: eax, edx, then on the stack (GCC specific) */
/* TODO: only tested with Intel (tanger could use another calling convention) */
	/* eax: tx, edx: attr, stack: __src */
#if defined(EXPLICIT_TX_PARAMETER)
	leal	4(%esp), %ecx     /* Get esp (stack pointer) */
	movl	%ecx, (%eax)      /* Save stack pointer */
	movl	(%esp), %ecx      /* Get return address */
	movl	%ebx, 4(%eax)     /* Save registers */
	movl	%esi, 8(%eax)
	movl	%edi, 12(%eax)
	movl	%ebp, 16(%eax)
	movl	%ecx, 20(%eax)    /* Save return address */
// NOTE third argument is not handle in this case
	call	GTM_begin_transaction
#else
	leal	4(%esp), %ecx
	subl	$28, %esp         /* Reserve space on the stack */
	.cfi_def_cfa_offset 32
	movl	%ecx, 8(%esp)
	movl	%ebx, 12(%esp)    /* Save registers */
	movl	%esi, 16(%esp)
	movl	%edi, 20(%esp)
	movl	%ebp, 24(%esp)
	leal	8(%esp), %edx
	call	GTM_begin_transaction
	addl	$28, %esp
#endif /* EXPLICIT_TX_PARAMETER  */
	.cfi_def_cfa_offset 4
	ret
#endif /* !LP64 */
	.cfi_endproc
	.size	_ITM_beginTransaction, .-_ITM_beginTransaction




	.p2align 4
	.globl	_ITM_siglongjmp
	.type	_ITM_siglongjmp, @function
	.hidden	_ITM_siglongjmp

_ITM_siglongjmp:
	.cfi_startproc
#ifdef __LP64__
	movq	(%rdi), %rcx
	movq	8(%rdi), %rdx
	movq	16(%rdi), %rbx   /* Restore registers */
	movq	24(%rdi), %rbp
	movq	32(%rdi), %r12
	movq	40(%rdi), %r13
	movq	48(%rdi), %r14
	movq	56(%rdi), %r15
	movl	%esi, %eax       /* 2nd paramter (esi) is returned value (eax, int, 32 bit) */
	.cfi_def_cfa %rcx, 0
	.cfi_register %rip, %rdx
	movq	%rcx, %rsp
	jmp	*%rdx
#else
	xchgl	%eax, %edx
	movl	(%edx), %ecx      /* esp (stack pointer) will be restored later */
	movl	4(%edx), %ebx     /* restore registers */
	movl	8(%edx), %esi
	movl	12(%edx), %edi
	movl	16(%edx), %ebp
	movl	20(%edx), %edx    /* eip (instruction pointer) will be restored after */
	.cfi_def_cfa %ecx, 0
	.cfi_register %eip, %edx
	movl	%ecx, %esp        /* restore esp, stack pointer */
	jmp	*%edx             /* restore eip, instruction pointer */
#endif
	.cfi_endproc
	.size	_ITM_siglongjmp, .-_ITM_siglongjmp

// TODO Defined this elsewhere
#if defined(TM_INTEL) && !defined(__PIC__) 

	.p2align 4
	.globl	malloc._$WrapTXN
	.type	malloc._$WrapTXN, @function
	.hidden	malloc._$WrapTXN
	.globl	malloc._$TXN
	.type	malloc._$TXN, @function
	.hidden	malloc._$TXN

malloc._$WrapTXN:
malloc._$TXN:
	.cfi_startproc
#ifdef __LP64__
	jmp _ITM_malloc
#else
	/* NOTE: unfortunately icc-32bit doesn't recognized it as regparm(2)
	   TODO: a way to defined it?
	*/
	movl 	4(%esp), %eax  /* Set 1st param */
	call 	_ITM_malloc
	movl 	%eax, 4(%esp)  /* Set return value */
	ret
#endif
	.cfi_endproc
	.size	malloc._$TXN, .-malloc._$TXN
	.size	malloc._$WrapTXN, .-malloc._$WrapTXN



	.p2align 4
	.globl	free._$WrapTXN
	.type	free._$WrapTXN, @function
	.hidden	free._$WrapTXN
	.globl	free._$TXN
	.type	free._$TXN, @function
	.hidden	free._$TXN

free._$WrapTXN:
free._$TXN:
	.cfi_startproc
#ifdef __LP64__
	jmp _ITM_free
#else
	/* NOTE: unfortunately icc-32bit doesn't recognized it as regparm(2)
	   TODO: a way to defined it?
	*/
	movl 	4(%esp), %eax  /* Set 1st param */
	call 	_ITM_free
	movl 	%eax, 4(%esp)  /* Set return value */
	ret
#endif
	.cfi_endproc
	.size	free._$WrapTXN, .-free._$WrapTXN
	.size	free._$TXN, .-free._$TXN

#endif

.section .note.GNU-stack, "", @progbits
