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
#include <spl.h>
#include <cpu.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>

/*
 * Dumb MIPS-only "VM system" that is intended to only be just barely
 * enough to struggle off the ground. You should replace all of this
 * code while doing the VM assignment. In fact, starting in that
 * assignment, this file is not included in your kernel!
 *
 * NOTE: it's been found over the years that students often begin on
 * the VM assignment by copying dumbvm.c and trying to improve it.
 * This is not recommended. dumbvm is (more or less intentionally) not
 * a good design reference. The first recommendation would be: do not
 * look at dumbvm at all. The second recommendation would be: if you
 * do, be sure to review it from the perspective of comparing it to
 * what a VM system is supposed to do, and understanding what corners
 * it's cutting (there are many) and why, and more importantly, how.
 */

/* under dumbvm, always have 72k of user stack */
/* (this must be > 64K so argument blocks of size ARG_MAX will fit) */
#define DUMBVM_STACKPAGES    18
static bool  vm_initialized = false;
/*
 * Wrap ram_stealmem in a spinlock.
 */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;
struct spinlock* cm_splock = NULL;

static struct coremap_entry* coremap = NULL;
paddr_t free_memory_start_address  = 0;
uint32_t coremap_count = 0;
uint32_t first_free_index = 0;
void
vm_bootstrap(void)
{
	/* Do nothing. */
	cm_splock = (struct spinlock*)kmalloc(sizeof(struct spinlock)); // stealing memory for this.
	spinlock_init(cm_splock);
	paddr_t last = ram_getsize();
	paddr_t first = ram_getfirstfree(); // cannot use firstpaddr and lastpaddr after this call.

//	first = first & PAGE_FRAME; // get it aligned
//	last  = last & PAGE_FRAME;
	kprintf("first free address : %d \n", first);

	coremap = (struct coremap_entry*)PADDR_TO_KVADDR(first);
	coremap_count = last/PAGE_SIZE;
	kprintf("coremap count : %d \n", coremap_count);

	uint32_t coremap_size = (coremap_count) * (sizeof(struct coremap_entry));
	kprintf("coremap size : %d \n", coremap_size);
	uint32_t fixed_memory_size = first + coremap_size;
	fixed_memory_size = ROUNDUP(fixed_memory_size, PAGE_SIZE);
	free_memory_start_address = fixed_memory_size;
	kprintf("free memory start address : %d \n", free_memory_start_address);

	first_free_index = (free_memory_start_address / PAGE_SIZE); 
	kprintf("free memory start index : %d \n", first_free_index);


	for(uint32_t i = 0; i < coremap_count; i++)
	{
		if(i < first_free_index)
			coremap[i].state = FIXED;
		else
			coremap[i].state = FREE;

		coremap[i].virtual_address = 0;
		coremap[i].chunks = -1;
		coremap[i].as = NULL;
	
	}

	vm_initialized = true;
}

/*
 * Check if we're in a context that can sleep. While most of the
 * operations in dumbvm don't in fact sleep, in a real VM system many
 * of them would. In those, assert that sleeping is ok. This helps
 * avoid the situation where syscall-layer code that works ok with
 * dumbvm starts blowing up during the VM assignment.
 */

/*
static
void
dumbvm_can_sleep(void)
{
	if (CURCPU_EXISTS()) {
		// must not hold spinlocks 
		KASSERT(curcpu->c_spinlocks == 0);

		 must not be in an interrupt handler 
		KASSERT(curthread->t_in_interrupt == 0);
	}
}
*/

static
paddr_t
getppages(unsigned long npages)
{
	paddr_t addr = 0;
	if(vm_initialized == false)
	{

		spinlock_acquire(&stealmem_lock);

		addr = ram_stealmem(npages);

		spinlock_release(&stealmem_lock);
	}
	else
	
	{

		//add new code here.
		if(first_free_index + npages >= coremap_count)
			addr = 0;


		else
		{

			spinlock_acquire(cm_splock);
	
	//		kprintf(" getpages called for %lu \n",npages);
			for(uint32_t i = first_free_index; i < coremap_count; i++)
			{
				if(coremap[i].state == FREE)
				{
					// check if we have n-1 more pages.
					unsigned long p = 0;
					uint32_t j = i + 1;
					while((p < (npages - 1)) && j < coremap_count)
					{
						if(coremap[j].state == FREE)
						{
							p++;
							j++;
						}
						else
						{
							break;
						}
					}
					if(p == npages - 1)
					{
						addr = i*PAGE_SIZE;
						
				//		kprintf(" allocating index %d \n", i);
						coremap[i].chunks = npages;

						for(unsigned long k = i; k < i+npages; k++)
						{
							// !!! Dirty should be set only when called from page_alloc. basically means that we need to update the disk.
							// When called from kmalloc, the page can never be swapped. so it should be FIXED/PINNED.
							coremap[k].state = FIXED;
					
						}
						break;
					}
					
				}
			}
			if(addr == 0)
				kprintf("cannot allocate %lu pages \n", npages);
			spinlock_release(cm_splock);
		}
			
	}
	

	return addr;
}

/* Allocate/free some kernel-space virtual pages */
vaddr_t
alloc_kpages(unsigned npages)
{
	paddr_t pa;

	pa = getppages(npages);
	if (pa==0) 
	{
		return 0;
	}

//	kprintf("*** allocating vaddr : %d \n *****", PADDR_TO_KVADDR(pa));
	return PADDR_TO_KVADDR(pa);
	
}

void
free_kpages(vaddr_t addr)
{
	if(vm_initialized == false)
		return;
//	kprintf("calling free on vaddr : %d \n", addr);	
	paddr_t paddr = addr - MIPS_KSEG0; // bigtime doubt here !!
// 	paddr_t paddr = KVADDR_TO_PADDR(addr);	
	uint32_t page_index = paddr/ PAGE_SIZE;
		
/*	if(page_index < first_free_index || page_index >= coremap_count)
	{
		return;
	}*/
//	else
	if(page_index > coremap_count)
		return;

	spinlock_acquire(cm_splock);

	//Should we allow the user to do this !!.. IMPORTANT - ASK TA.
	//km2 and km5 are asking me to free memory that they have not called to be allocated. Is this fine ??
	if(page_index < first_free_index)
	{

		kprintf("*** freeing a page which was allocated with ram_stealmem() *****");
		coremap[page_index].state = FREE;
		coremap[page_index].chunks = -1;
	
	}
	else
	{
	//	spinlock_acquire(cm_splock);
		int chunks = coremap[page_index].chunks;
		
		
	//	kprintf(" freeing index1 : %d \n", page_index);
		if(coremap[page_index].state != FIXED)
			kprintf("*** calling free on a non dirty page ****\n");
		while(chunks > 0 && (page_index >= first_free_index) && (page_index < coremap_count))
		{
			if(coremap[page_index].state != FIXED)
			{
				kprintf("*** freeing a non dirty page **** \n");
			
			}
	//		kprintf("freeing page : %d \n", page_index);
			coremap[page_index].state = FREE;
			coremap[page_index].chunks = -1; // sheer paranoia.
			as_zero_region(page_index*PAGE_SIZE, 1);
			chunks--;
			page_index++;
			
		}
	//	spinlock_release(cm_splock);
		
	}
	spinlock_release(cm_splock);

}

unsigned
int
coremap_used_bytes() {

	/* dumbvm doesn't track page allocations. Return 0 so that khu works. */

	unsigned int count = 0;
	spinlock_acquire(cm_splock);
	for(uint32_t i = 0; i < coremap_count; i++)
	{
		if(coremap[i].state == FREE)
			count++;	
	}
	spinlock_release(cm_splock);
	return (coremap_count-count)*PAGE_SIZE;
}

void
vm_tlbshootdown_all(void)
{
	panic("dumbvm tried to do tlb shootdown?!\n");
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("dumbvm tried to do tlb shootdown?!\n");
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	(void)faulttype;
	(void)faultaddress;
	return 0;

	if(curproc == NULL)
		return EFAULT;
	
	struct addrspace *as = NULL;
	as = proc_getas();
	if(as == NULL)
		return EFAULT;
	

	//check if fault_address is valid.
	faultaddress &= PAGE_FRAME;
	struct list_node* region = as->as_region_list;

	bool is_valid = false;
	int can_read = 0, can_write = 0, can_execute = 0;

	while(region != NULL)
	{
		struct as_region* temp = (struct as_region*)region->node;
		vaddr_t vaddr = temp->region_base;
		size_t npages = temp->region_npages;
	
	 	// currently checking in regions provided by the ELF, should check in stack and heap also. HOW??
		if(faultaddress >= vaddr  && faultaddress < vaddr + PAGE_SIZE*npages)
		{
			is_valid = true;
			can_read = temp->can_read;
			can_write = temp->can_write;
			can_execute = temp->can_execute;
			
			break;
		}
		region = region->next;
	}

	if(is_valid == false)
	{
		// kill the process?
		return EFAULT;
	
	}
	(void)can_read;
	(void)can_write;
	(void)can_execute;



	switch(faulttype)
	{
		case VM_FAULT_READONLY:
		{
		
		
		
			break;
		}

		case VM_FAULT_READ:
		{
		
		
		
			break;
		}
		case VM_FAULT_WRITE:
		{
		
		
		
			break;
		}
		
		default:
		return EINVAL;
	
	
	}



/*
	vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	paddr_t paddr;
	int i;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;

	faultaddress &= PAGE_FRAME;

	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
	    case VM_FAULT_READONLY:
		// We always create pages read-write, so we can't get this 
		panic("dumbvm: got VM_FAULT_READONLY\n");
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
		return EINVAL;
	}

	if (curproc == NULL) {
		
		// No process. This is probably a kernel fault early
		// in boot. Return EFAULT so as to panic instead of
		// getting into an infinite faulting loop.
		 
		return EFAULT;
	}

	as = proc_getas();
	if (as == NULL) {
		
		 // No address space set up. This is probably also a
		 //kernel fault early in boot.
		 
		return EFAULT;
	}

	 //Assert that the address space has been set up properly. 
	KASSERT(as->as_vbase1 != 0);
	KASSERT(as->as_pbase1 != 0);
	KASSERT(as->as_npages1 != 0);
	KASSERT(as->as_vbase2 != 0);
	KASSERT(as->as_pbase2 != 0);
	KASSERT(as->as_npages2 != 0);
	KASSERT(as->as_stackpbase != 0);
	KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
	KASSERT((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
	KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
	KASSERT((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
	KASSERT((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);

	vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;

	if (faultaddress >= vbase1 && faultaddress < vtop1) {
		paddr = (faultaddress - vbase1) + as->as_pbase1;
	}
	else if (faultaddress >= vbase2 && faultaddress < vtop2) {
		paddr = (faultaddress - vbase2) + as->as_pbase2;
	}
	else if (faultaddress >= stackbase && faultaddress < stacktop) {
		paddr = (faultaddress - stackbase) + as->as_stackpbase;
	}
	else {
		return EFAULT;
	}

	// make sure it's page-aligned 
	KASSERT((paddr & PAGE_FRAME) == paddr);

	// Disable interrupts on this CPU while frobbing the TLB. 
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		}
		ehi = faultaddress;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;
	}

	kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n");
	splx(spl);
	return EFAULT;
	*/
}

/*
paddr_t get_physical_page_address(struct addrspace*as, vaddr_t vaddr)
{
	paddr_t paddr = 0;
	if(as == NULL)
		return 0;
	
	struct list_node* page_table = as->as_page_list;





}*/

