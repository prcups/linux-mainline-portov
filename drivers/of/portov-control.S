/* SPDX-License-Identifier: GPL-2.0-only */
#include <asm-generic/vmlinux.lds.h>

	.section .rodata,"a"
	.balign STRUCT_ALIGNMENT
	.global __dtb_portov_control_begin
__dtb_portov_control_begin:
	.incbin "drivers/of/portov-control.dtb"
	.global __dtb_portov_control_end
__dtb_portov_control_end:
	.balign STRUCT_ALIGNMENT

	.section .note.GNU-stack,"",%progbits
