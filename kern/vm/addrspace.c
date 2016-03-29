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
/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

extern struct spinlock* cm_splock;
struct addrspace *
as_create(void)
{
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
	as->as_stack_end = 0;
	as->as_heap_start = 0;
	as->as_heap_end = 0;
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

	/*
	 * Write this.
	 */

	struct list_node* temp1 = old->as_region_list;
	while(temp1 != NULL)
	{
		struct as_region* r_new = (struct as_region*)kmalloc(sizeof(struct as_region));
		if(r_new == NULL)
			return ENOMEM;

		struct as_region* r_old = (struct as_region*)temp1->node;
		r_new->region_base = r_old->region_base;
		r_new->region_npages = r_old->region_npages;
		r_new->can_read = r_old->can_read;
		r_new->can_write = r_old->can_write;
		r_new->can_execute = r_old->can_execute; 

		int ret = add_node(&newas->as_region_list,r_new); 
		if(ret == -1)
			return ENOMEM;
		
		temp1 = temp1->next;
	}

	temp1 = old->as_page_list;
	spinlock_acquire(cm_splock);
	while(temp1 != NULL)
	{
		struct page_table_entry* p_new = (struct page_table_entry*)kmalloc(sizeof(struct page_table_entry));
		if(p_new == NULL)
			return ENOMEM;

		struct page_table_entry* p_old = (struct page_table_entry* )temp1->node;
		p_new->vaddr = p_old->vaddr; // virtual addresses can be the same, no issue there.

		// Now, how do i copy the actual page,
		// dumbvm calls memmove. I can't do the same because we have to allcoate some physical memory here.
		// But all physical memory allocations should happen at vm_fault.

//		p_new->paddr = p_old->paddr; // this is wrong, they do not map to the same physical address..
		p_new->can_read = p_old->can_read;
		p_new->can_write = p_old->can_write;
		p_new->can_execute = p_old->can_execute;

		void *page = kmalloc(PAGE_SIZE); // but this returns kernel virtual address. but that's ok, cause we are mapping it to a user space virtual address.
										// owner gets set as the kernel though. This probably is trouble when swapping. Change the coremap value ??
										// or not, just set it to curproc in the kmalloc code.
		memmove(page,
			(const void *)PADDR_TO_KVADDR(p_old->paddr), //use this? or PADDR_TO_KVADDR like dumbvm does?. But why does dumbvm do that in the first place.
			PAGE_SIZE);									// i know why, cannot call functions on user memory addresses. So convert it into a kv address.
														// the function will translate it into a physical address again and free it. ugly Hack. but no other way.

		p_new->paddr = KVADDR_TO_PADDR((vaddr_t)page);
        

		int ret = add_node(&newas->as_page_list,p_new);
		if(ret == -1)
		{
			spinlock_release(cm_splock);
			return ENOMEM;
		}
		temp1 = temp1->next;
	
	}
	spinlock_release(cm_splock);

	(void)old;

	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	/*
	 * Clean up as needed.
	 */
	
	// need to free the individual pages as well, that's why this is not as straight forward
	struct list_node* cur = as->as_page_list;
	struct list_node* next = NULL;
	while(cur != NULL)
	{
		next = cur->next;
		struct page_table_entry* p = cur->node;
		kfree((void *)PADDR_TO_KVADDR(p->paddr)); // is this safe?.Using this because you cannot kfree a user virtual address.
											 	 // so we pass the KV address corresponding to the physical address.
		kfree(p);
		kfree(cur);
		cur = next;
	
	}

	//this is straight forward though.
	delete_list(&(as->as_region_list));
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
	temp->can_read = readable;
	temp->can_write = writeable;
	temp->configured_can_write = writeable; // store this. see as_prepare_load and as_complete_load to see why this is needed
	temp->can_execute = executable;

	//setting heap address
	if(vaddr + (npages*PAGE_SIZE) > as->as_heap_start)
	{
		as->as_heap_start = vaddr + (npages*PAGE_SIZE);
		as->as_heap_start = ROUNDUP(as->as_heap_start, PAGE_SIZE);
	}

    int ret = add_node(&(as->as_region_list), temp);
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
	
	struct list_node *temp = as->as_region_list;
	if(temp == NULL)
		return EINVAL;
	
	while(temp != NULL)
	{
		struct as_region* temp1 = (struct as_region*)temp->node;
		if(temp1 == NULL)
		{
			return EINVAL;
		
		}
		temp1->can_write = 1; // doing this so that the segments can be loaded into memory. will set it back to proper values in as_complete_load.
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
	
	struct list_node *temp = as->as_region_list;
	if(temp == NULL)
		return EINVAL;
	
	while(temp != NULL)
	{

		struct as_region* temp1 = (struct as_region*)temp->node;
		if(temp1 == NULL)
		{
			return EINVAL;
		
		}
		temp1->can_write = temp1->configured_can_write;
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

