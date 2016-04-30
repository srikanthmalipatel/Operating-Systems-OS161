/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>
#include <spl.h>
#include <mips/tlb.h>
#include <thread.h>
/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

//extern struct spinlock* cm_splock;
extern vaddr_t copy_buffer_vaddr;
struct lock* as_lock = NULL;
extern struct coremap_entry* coremap;

void
as_zero_region(paddr_t paddr, unsigned npages)
{
	bzero((void *)PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
}

struct addrspace *
as_create(void)
{
	static bool first = true;
	if(first == true)
	{
		first = false;
		as_lock = lock_create("as lock");
		KASSERT(as_lock != NULL);
	}
	
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	/*
	 * Initialize as needed.
	 */
	as->as_page_list = NULL;
	as->as_region_list = NULL;
//	as->as_stack_end = 0;
	as->as_heap_start = 0;
	as->as_heap_end = 0;
	//as->as_lock = lock_create("as lock");
//	if(as->as_lock == NULL)
//		return NULL;
//	as->as_splock = kmalloc(sizeof(struct spinlock));
//	spinlock_init(as->as_splock);
	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;

	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}

//	kprintf(" **** inside as copy ****  \n");
//	spinlock_acquire(newas->as_splock);
//	spinlock_acquire(old->as_splock);
//	lock_acquire(newas->as_lock);
//	lock_acquire(old->as_lock);
	lock_acquire(as_lock);
	struct as_region* r_old = old->as_region_list;
	while(r_old != NULL)
	{
		struct as_region* r_new = (struct as_region*)kmalloc(sizeof(struct as_region));
		if(r_new == NULL)
		{
			lock_release(as_lock);
		//	lock_release(old->as_lock);
		//	lock_release(newas->as_lock);
			//spinlock_release(old->as_splock);
			//spinlock_release(newas->as_splock);
			as_destroy(newas);
			return ENOMEM;
		}

		r_new->region_base = r_old->region_base;
		r_new->region_npages = r_old->region_npages;
		r_new->can_read = r_old->can_read;
		r_new->can_write = r_old->can_write;
		r_new->can_execute = r_old->can_execute; 

		int ret = region_list_add_node(&newas->as_region_list,r_new); 
		if(ret == -1)
		{
			lock_release(as_lock);
			//lock_release(old->as_lock);
			//lock_release(newas->as_lock);
		//	spinlock_release(old->as_splock);
		//	spinlock_release(newas->as_splock);
			as_destroy(newas);
			return ENOMEM;
		}
		r_old = r_old->next;
	}

	struct page_table_entry* p_old = old->as_page_list;
	while(p_old != NULL)
	{
		struct page_table_entry* p_new = (struct page_table_entry*)kmalloc(sizeof(struct page_table_entry));
		if(p_new == NULL)
		{
			lock_release(as_lock);	
		//	lock_release(old->as_lock);
		//	lock_release(newas->as_lock);
	//		spinlock_release(old->as_splock);
	//		spinlock_release(newas->as_splock);
			as_destroy(newas);
			
			return ENOMEM;
		}
		p_new->vaddr = p_old->vaddr;
		p_new->swap_pos = -1;

		KASSERT(p_old->page_state != SWAPPING);
		while(p_old->page_state == SWAPPING)
		{
	//		if(spinlock_do_i_hold(old->as_splock))
	//			spinlock_release(old->as_splock);
	//		if(spinlock_do_i_hold(newas->as_splock))
	//			spinlock_release(newas->as_splock);

			thread_yield();
		
		}
//		if(!spinlock_do_i_hold(newas->as_splock))
//			spinlock_acquire(newas->as_splock);
//		if(!spinlock_do_i_hold(old->as_splock))
//			spinlock_acquire(old->as_splock);

	//	if(!spinlock_do_i_hold)
	//	KASSERT(p_old->page_state != SWAPPING);

		if(p_old->page_state == SWAPPED)
		{
			// this page is in disk, so we need to create a copy of that page somewhere in disk and then update the page table entry of the new process.
			// going with the disk->memory->disk approach suggested in a recitation video by jinghao shi.
			// Allocate a buffer at vm_bootstrap of size 4k (1 page). Use this buffer to temporarily copy data from disk to here and then to disk again
			// then clear the buffer. This buffer is a shared resource, so we need a lock around it.

		//	kprintf("in as_copy swap code \n");
		//	spinlock_release(old->as_splock);
		//	spinlock_release(newas->as_splock);
			swap_in(p_old->vaddr,old,copy_buffer_vaddr, p_old->swap_pos);
		//	kprintf("completed swap in \n");
			int pos = mark_swap_pos(p_new->vaddr, newas);
			KASSERT(pos != -1);
			int err = write_to_disk(KVADDR_TO_PADDR(copy_buffer_vaddr)/PAGE_SIZE, pos);
		//	kprintf("completed writing to disk \n");
			KASSERT(err == 0);
	//		spinlock_acquire(newas->as_splock);
	//		spinlock_acquire(old->as_splock);
			as_zero_region(KVADDR_TO_PADDR(copy_buffer_vaddr),1);
			p_new->page_state = SWAPPED;
			p_new->swap_pos = pos;
			p_new->paddr = 0;
		}
		else
		{
			KASSERT(p_old->page_state == MAPPED);
			paddr_t paddr = get_user_page(p_old->vaddr, false, newas);
			KASSERT(p_old->page_state == MAPPED);
		//	int spl = splhigh();
			if(lock_do_i_hold(as_lock) == false)
				lock_acquire(as_lock);
			if(paddr == 0)
			{
				lock_release(as_lock);			
	//		lock_release(old->as_lock);
	//		lock_release(newas->as_lock);
//				spinlock_release(old->as_splock);
//				spinlock_release(newas->as_splock);
				as_destroy(newas);
				return ENOMEM;
			}
			uint32_t old_index = p_old->paddr/PAGE_SIZE;
			KASSERT(coremap[old_index].is_victim == false);
			KASSERT(coremap[paddr/PAGE_SIZE].is_victim == false);
			memmove((void*)PADDR_TO_KVADDR(paddr),
				(const void *)PADDR_TO_KVADDR(p_old->paddr), //use this? or PADDR_TO_KVADDR like dumbvm does?. But why does dumbvm do that in the first place.
				PAGE_SIZE);									// i know why, cannot call functions on user memory addresses. So convert it into a kv address.
														// the function will translate it into a physical address again and free it. ugly Hack. but no other way.

			p_new->paddr = paddr;
			p_new->page_state = MAPPED;
        
        //	splx(spl);

			int ret = page_list_add_node(&newas->as_page_list,p_new);
			if(ret == -1)
			{
				lock_release(as_lock);			
	//		lock_release(old->as_lock);
	//		lock_release(newas->as_lock);
	//			spinlock_release(old->as_splock);
	//			spinlock_release(newas->as_splock);
				as_destroy(newas);
				return ENOMEM;
			}
			p_old = p_old->next;
		}
	
	}

	newas->as_heap_start = old->as_heap_start;
	newas->as_heap_end = old->as_heap_end;
	*ret = newas;


	lock_release(as_lock);
//	lock_release(old->as_lock);
//	lock_release(newas->as_lock);
//	spinlock_release(old->as_splock);
//	spinlock_release(newas->as_splock);
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	KASSERT(as != NULL);	
	struct page_table_entry* cur = as->as_page_list;
	struct page_table_entry* next = NULL;
	lock_acquire(as_lock);
//	lock_acquire(as->as_lock);
//	spinlock_acquire(as->as_splock);
	while(cur != NULL)
	{
		next = cur->next;
		KASSERT(cur->page_state != SWAPPING);
		while(cur->page_state == SWAPPING)
		{
		//	if(spinlock_do_i_hold(as->as_splock))
		//		spinlock_release(as->as_splock);
			thread_yield();
		}

	//	if(!spinlock_do_i_hold(as->as_splock))
	//		spinlock_acquire(as->as_splock);

		bool is_swapped = false;
		if(cur->page_state == SWAPPED)
		{
		//	kprintf("as destroy , swapped is true \n");
			is_swapped = true;
		}	
		free_user_page(cur->vaddr,cur->paddr,as, false, is_swapped, cur->swap_pos);
		kfree(cur);
		cur = next;
	}
//	page_list_delete(&(as->as_page_list));
	as->as_page_list = NULL;

	//this is straight forward though.
	region_list_delete(&(as->as_region_list));
	as->as_region_list = NULL;
	lock_release(as_lock);
//	lock_release(as->as_lock);
//	lock_destroy(as->as_lock);
//	spinlock_release(as->as_splock);
//	spinlock_cleanup(as->as_splock);
//	kfree(as->as_splock);
	kfree(as);
}

void
as_activate(void)
{
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		/*
		 * Kernel thread without an address space; leave the
		 * prior address space in place.
		 */
		return;
	}

	/*
	 * Write this.
	 */

	int i, spl;
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
		 int readable, int writeable, int executable)
{
	/*
	 * Write this.
	 */

	if(as == NULL)
		return EINVAL;

	// lifted from dumbvm, hopefully is correct	
	// Aligning the base.
	memsize += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	memsize = (memsize + PAGE_SIZE - 1) & PAGE_FRAME;

  // Should check for overlaps too.but how
	size_t npages = memsize / PAGE_SIZE;

	struct as_region* temp = (struct as_region*)kmalloc(sizeof(struct as_region));
	temp->region_base = vaddr;
	temp->region_npages = npages;
	if(readable == 4)
		temp->can_read = 1;
	else
		temp->can_read = 0;
	
	if(writeable == 2)
		temp->can_write = 1;
	else
		temp->can_write = 0;
	if(executable == 1)
	{
		as->code_start = temp->region_base;
		as->code_end = as->code_start + npages*PAGE_SIZE;
		temp->can_execute = 1;
	}
	else
		temp->can_execute = 0;
	temp->configured_can_write = temp->can_write; // store this. see as_prepare_load and as_complete_load to see why this is needed

	//setting heap address
	if(vaddr + (npages*PAGE_SIZE) > as->as_heap_start)
	{
		as->as_heap_start = vaddr + (npages*PAGE_SIZE);
		as->as_heap_start = ROUNDUP(as->as_heap_start, PAGE_SIZE); // Alignment
		as->as_heap_end = as->as_heap_start; // just initializing, subsequent calls to sbrk will alter this value.
	}

    int ret = region_list_add_node(&(as->as_region_list), temp);
	if(ret == -1)
		return ENOMEM;

	return 0;
}

int
as_prepare_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */
	if(as == NULL)
		return EINVAL;
	
	struct as_region *temp = as->as_region_list;
	if(temp == NULL)
		return EINVAL;
	
	while(temp != NULL)
	{
		temp->can_write = 1; // doing this so that the segments can be loaded into memory. will set it back to proper values in as_complete_load.
		temp = temp->next;
	}
	
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */
	if(as == NULL)
		return EINVAL;
	
	struct as_region *temp = as->as_region_list;
	if(temp == NULL)
		return EINVAL;
	
	while(temp != NULL)
	{

		temp->can_write = temp->configured_can_write;
		temp = temp->next;
	}

	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	/*
	 * Write this.
	 */

	(void)as;

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	return 0;
}

