/*
 *  DA probe
 *
 * Copyright (c) 2000 - 2014 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 *
 * Vitaliy Cherepanov <v.cherepanov@samsung.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Contributors:
 * - Samsung RnD Institute Russia
 *
 */

/*
 * function
 *  <some ret type> _da_call_original(void *funcp, char *args[], int args_count);
 *
 *  arguments:
 *	funcp       - function pointer address to call
 *	args        - params array for function
 *	args_count  - params count
 *  return values:
 *	depends on original function (funcp)
 *
 *  use this function to call original function with variable arguments count
 *  in ld-preloaded probes
 *
 *  example:
 *	#include <dlfcn.h>
 *	#include <dlsym.h>
 *
 *	typedef int (*execl_type)(const char *path, const char *arg, ...);
 *	static execl_type execl_real;
 *	//....
 *	//_da_call_original function prototype
 *	int call_original(void *funcp, char *args[], int args_count);
 *	//...
 *	execl_real = (execl_type)dlsym(RTLD_NEXT , "execl");
 *	//...
 *	//probe execl
 *	int execl(const char *path, const char *arg, ...)
 *	{
 *		int res = 0;
 *		va_list ap;
 *		int args_count = 0, i;
 *
 *		//additional argumets list terminates with NULL pointer on execl call
 *		//calculate arguments count with terminate NULL
 *		va_start(ap, arg);
 *		do
 *			args_count++;
 *		while (va_arg(ap, char *) != 0);
 *		va_end(ap);
 *
 *		//add 2 for <path> and <arg> params
 *		args_count += 2;
 *
 *		//create and fill args arras
 *		char *all_args[args_count];
 *
 *		va_start(ap, arg);
 *
 *		all_args[0] = (char *)path;
 *		all_args[1] = (char *)arg;
 *		for (i = 2; i < args_count; i++)
 *			all_args[i] = va_arg(ap, char *);
 *		//do something
 *		//.....
 *		//call original function
 *		res = _da_call_original(&execl_real, all_args, args_count);
 *		//do something
 *		//......
 *		return res;
 *
 *	}
 *
 *  this function is written in asm because there is no other way to prepare
 *  variable count of arguments for original function call
 */

#define stack_size 0x40

#if defined(__x86_64__)
    /* x86 64-bit ----------------------------------------------- */

.text
.global _da_call_original
.type _da_call_original, @function
_da_call_original:
	mov	$-1, %rax
	ret
#elif defined(__i386)

.text
.global _da_call_original
.type _da_call_original, @function
_da_call_original:
	#save stack position
	push %ebp
	mov  %esp, %ebp

	#create local stack
	sub  $stack_size, %esp

	#store registers
	push %ebx
	push %ecx
	push %edx

	#load args array pointer
	movl 0xc(%ebp),%ecx
	#load args_count
	movl 0x10(%ebp), %ebx

	#calculate stack size for original function call
	#stack size = (args_count) * 4
	shl  $2, %ebx
	#create stack for original function call
	sub  %ebx, %esp

	#push all arguments to stack
	#for (i = 0; i < args_count; i++) {
	movl $0, %eax
	jmp  for_i_cmp
	for_i:
		#args[i] -> stack
		mov  (%ecx), %edx
		mov  %edx, (%esp)
		#stack++
		add  $4, %esp
		add  $4, %ecx
		add  $4, %eax
	for_i_cmp:
	cmpl %ebx, %eax
	jl   for_i
	#}

	#shift stack back
	sub  %ebx, %esp

	#call original function (funcp)
	movl 8(%ebp), %eax
	call *(%eax)
	#now function result in %eax

	#restore stack after call function
	add  %ebx, %esp

	#restore registers
	pop  %edx
	pop  %ecx
	pop  %ebx

	#restore local stack
	add  $stack_size, %esp
	#return
	pop  %ebp
	ret

#elif defined(__arm__)
.text
.global _da_call_original
.type _da_call_original, %function

_da_call_original:
	push {fp, lr, r5, r6, r7, r8}
	#create local stack
	add  fp, sp, #4
	sub  sp, sp, #stack_size
	#store params to stack
	str  r0, [fp, #-12]
	str  r1, [fp, #-16]
	str  r2, [fp, #-20]

	#load args array pointer
	ldr  r5, [fp, #-16]
	#load args_count
	ldr  r6, [fp, #-20]

	#calculate stack size for original function call
	#stack size = (args_count) * 4
	lsl  r6, r6, #2
	#create stack for original function call
	sub  sp, sp, r6
	#first 4 params pass throw r0-r3
	#so move stack to 4 words
	sub  sp, sp, #16

	#for (i = 4; i < arg; i++) {
	mov  r7, #16
	b    for_i_cmp
	for_i:
		#args[i] -> stack[i]
		ldr  r8, [r5, r7]
		str  r8, [sp, r7]
		add  r7, r7, #4
	for_i_cmp:
	cmp  r7, r6
	blt  for_i
	#}

	#move stack pointer back to 4 words
	add  sp, sp, #16
	#load first 4 args
	ldr  r0, [r5, #0 ]
	ldr  r1, [r5, #4 ]
	ldr  r2, [r5, #8 ]
	ldr  r3, [r5, #12]

	#call original function (funcp)
	ldr  r5, [fp, #-12]
	ldr  r5, [r5]
	blx  r5
	#now function result in r0

	#restore stack after original fincion call
	add  sp, sp, r6

	#local stack restore
	add  sp, sp, #stack_size
	#return
	pop  {fp, pc, r5, r6, r7, r8}
#endif
