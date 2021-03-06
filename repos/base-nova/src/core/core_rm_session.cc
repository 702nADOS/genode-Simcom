/*
 * \brief  Core-local RM session
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>

/* core includes */
#include <core_rm_session.h>
#include <platform.h>
#include <util.h>
#include <nova_util.h>

/* NOVA includes */
#include <nova/syscalls.h>

using namespace Genode;

/**
 * Map dataspace core-locally
 */
static inline void * alloc_region(Dataspace_component *ds, const size_t size)
{
	/*
	 * Allocate range in core's virtual address space
	 *
	 * Start with trying to use natural alignment. If this does not work,
	 * successively weaken the alignment constraint until we hit the page size.
	 */
	void *virt_addr = 0;
	size_t align_log2 = log2(ds->size());
	for (; align_log2 >= get_page_size_log2(); align_log2--) {
		if (platform()->region_alloc()->alloc_aligned(size,
		                                              &virt_addr, align_log2).is_ok())
			break;
	}

	return virt_addr;
}

Rm_session::Local_addr
Core_rm_session::attach(Dataspace_capability ds_cap, size_t size,
                        off_t offset, bool use_local_addr,
                        Rm_session::Local_addr local_addr,
                        bool executable)
{
	auto lambda = [&] (Dataspace_component *ds) -> Local_addr {
		if (!ds)
			throw Invalid_dataspace();

		if (use_local_addr) {
			PERR("Parameter 'use_local_addr' not supported within core");
			return nullptr;
		}

		if (offset) {
			PERR("Parameter 'offset' not supported within core");
			return nullptr;
		}

		const size_t page_rounded_size = align_addr(ds->size(), get_page_size_log2());

		/* allocate the virtual region contiguous for the dataspace */
		void * virt_ptr = alloc_region(ds, page_rounded_size);
		if (!virt_ptr)
			throw Out_of_metadata();

		/* map it */
		Nova::Utcb * const utcb = reinterpret_cast<Nova::Utcb *>(Thread_base::myself()->utcb());
		const Nova::Rights rights(true, ds->writable(), executable);

		if (map_local(utcb, ds->phys_addr(), reinterpret_cast<addr_t>(virt_ptr),
		              page_rounded_size >> get_page_size_log2(), rights, true)) {
			platform()->region_alloc()->free(virt_ptr, page_rounded_size);
			throw Out_of_metadata();
		}

		return virt_ptr;
	};
	return _ds_ep->apply(ds_cap, lambda);
}


void Core_rm_session::detach(Local_addr core_local_addr)
{
	size_t size = platform_specific()->region_alloc_size_at(core_local_addr);

	unmap_local(reinterpret_cast<Nova::Utcb *>(Thread_base::myself()->utcb()),
	            core_local_addr, size >> get_page_size_log2());

	platform()->region_alloc()->free(core_local_addr);
}
