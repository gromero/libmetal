/*
 * Copyright (c) 2022, Xilinx Inc. and Contributors. All rights reserved.
 * Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * @file	generic/xlnx_common/zynqmp_aarch64/sys.c
 * @brief	machine specific system primitives implementation.
 */

#include <metal/compiler.h>
#include <metal/io.h>
#include <metal/sys.h>
#include <stdint.h>
#include "xil_cache.h"
#include "xil_exception.h"
#include "xil_mmu.h"
#include "xscugic.h"

#ifdef VERSAL_NET
#include "xcpu_cortexa78.h"
#elif defined(versal)
#include "xcpu_cortexa72.h"
#else
#include "xreg_cortexa53.h"
#endif /* defined(versal) */

void sys_irq_restore_enable(unsigned int flags)
{
	Xil_ExceptionEnableMask(~flags);
}

unsigned int sys_irq_save_disable(void)
{
	unsigned int state = mfcpsr() & XIL_EXCEPTION_ALL;

	if (state != XIL_EXCEPTION_ALL)
		Xil_ExceptionDisableMask(XIL_EXCEPTION_ALL);

	return state;
}

void metal_machine_cache_flush(void *addr, unsigned int len)
{
	if (!addr && !len)
		Xil_DCacheFlush();
	else
		Xil_DCacheFlushRange((intptr_t)addr, len);
}

void metal_machine_cache_invalidate(void *addr, unsigned int len)
{
	if (!addr && !len)
		Xil_DCacheInvalidate();
	else
		Xil_DCacheInvalidateRange((intptr_t)addr, len);
}

/**
 * @brief poll function until some event happens
 */
void metal_weak metal_generic_default_poll(void)
{
	metal_asm volatile("wfi");
}

void *metal_machine_io_mem_map(void *va, metal_phys_addr_t pa,
			       size_t size, unsigned int flags)
{
	unsigned long section_offset;
	unsigned long ttb_addr;
#if defined(__aarch64__)
	unsigned long ttb_size = (pa < 4 * GB) ? 2 * MB : 1 * GB;
#else
	unsigned long ttb_size = 1 * MB;
#endif /* defined(__aarch64__) */

	if (!flags)
		return va;

	/* Ensure alignment on a section boundary */
	pa &= ~(ttb_size - 1UL);

	/*
	 * Loop through entire region of memory (one MMU section at a time).
	 * Each section requires a TTB entry.
	 */
	for (section_offset = 0; section_offset < size; ) {
		/* Calculate translation table entry for this memory section */
		ttb_addr = (pa + section_offset);

		/* Write translation table entry value to entry address */
		Xil_SetTlbAttributes(ttb_addr, flags);

#if defined(__aarch64__)
		/*
		 * recalculate if we started below 4GB and going above in
		 * 64bit mode
		 */
		if (ttb_addr >= 4 * GB)
			ttb_size = 1 * GB;
#endif
		section_offset += ttb_size;
	}

	return va;
}
