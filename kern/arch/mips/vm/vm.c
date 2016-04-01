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
struct spinlock* tlb_splock = NULL;
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
	tlb_splock = (struct spinlock*)kmalloc(sizeof(struct spinlock));
	spinlock_init(tlb_splock);
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

							// No. kernel threads can call kmalloc, set owner as current process. So can be dirty, just need to check if it is kernel to set FIXED.
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
//	paddr_t paddr = addr - MIPS_KSEG0; // bigtime doubt here !!
 	paddr_t paddr = KVADDR_TO_PADDR(addr);	
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


			// free this page from the processes page table, do this only if cur proc is not kernel
			// there's probably a better way to do this.


			/// **** ALSO CLEAR THE TLB ENTRY FOR THIS PAGE ******
			if(curproc->pid != 1)
		//	if(strcmp(curproc->p_name,"[kernel]") != 0 || curproc->pid != 1)
			{
				struct addrspace* as = NULL;
				as = proc_getas();
				KASSERT(as != NULL);
		//*********** check this again ***********//
			/*	if(as == NULL)
				{
					page_index++;
					chunks--;
					continue;
				}
				*/
					
				struct list_node* temp = as->as_page_list;
		/*		if(temp == NULL)
				{
					page_index++;
					chunks--;
					continue;
				}*/
				KASSERT(temp!= NULL);

				struct list_node* prev = NULL;
				struct page_table_entry* p = (struct page_table_entry*)temp->node;
				KASSERT(p != NULL);
				if(p->paddr == page_index*PAGE_SIZE)
				{
					kfree(p);
					as->as_page_list = temp->next;	
					kfree(temp);
	
				}
				else
				{
					while(temp != NULL)
					{
						struct page_table_entry* p = (struct page_table_entry*)temp->node;
						KASSERT(p != NULL);
						if(p->paddr == page_index*PAGE_SIZE)
							{
							KASSERT(prev != NULL);
							prev->next = temp->next;
							kfree(p);
							kfree(temp);
							break;
				
						}
						else
						{
							prev = temp;
							temp = temp->next;
						}
	
	
					}
				}

				spinlock_acquire(tlb_splock);
				int i, spl;
				spl = splhigh();
				uint32_t elo,ehi;
				for (i=0; i<NUM_TLB; i++) 
				{
					tlb_read(&ehi, &elo, i);
					if (elo & TLBLO_VALID) 
					{
						elo &= PAGE_SIZE; // removing the other meta bits.
						if(elo == page_index* PAGE_SIZE)
						{
							tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
							break;	
						}
					}
				}

				splx(spl);

				spinlock_release(tlb_splock);


			}

			chunks--;
			page_index++;
			
		}
		
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

unsigned int coremap_free_bytes()
{
   unsigned int free = 0;
   
   spinlock_acquire(cm_splock);
   for(uint32_t i = 0; i < coremap_count; i++)
   {
	  if(coremap[i].state == FREE)
		  free++;	
   }
   spinlock_release(cm_splock);
   return free*PAGE_SIZE;
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
/*	if(faultaddress == USERSTACK - PAGE_SIZE)
	{
	    faultaddress &= PAGE_FRAME;
		faultaddress &= PAGE_FRAME;	
	}
	*/
	if(faulttype != VM_FAULT_READ && faulttype != VM_FAULT_WRITE && faulttype != VM_FAULT_READONLY)
		return EINVAL;

	if(curproc == NULL)
		return EFAULT;
	
	struct addrspace *as = NULL;
	as = proc_getas();
	if(as == NULL)
		return EFAULT;
	

//	spinlock_acquire(tlb_splock);
	//check if fault_address is valid.
	faultaddress &= PAGE_FRAME;
	struct list_node* region = as->as_region_list;

	bool is_valid = false;
	int can_read = 0, can_write = 0, can_execute = 0;
	bool is_stack_page = false, is_heap_page = false;

	while(region != NULL)
	{
		struct as_region* temp = (struct as_region*)region->node;
		KASSERT(temp != NULL);
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
		// check heap. heap_start is setup in as_define_region, heap_end is set in sbrk.
		// each process contains one thread only, so we dont have to synchronize here.
		if(faultaddress >= as->as_heap_start && faultaddress < as->as_heap_end)
		{	
			is_valid = true;
			is_heap_page = true;
			can_read = 1;
			can_write = 1;
			can_execute = 1; // really though?
		}
	}

	if(is_valid == false)
	{
		// check stack.  
		if(faultaddress >= VM_STACKBOUND && faultaddress <= USERSTACK - PAGE_SIZE)
		{	
			is_valid = true;
			is_stack_page = true;
			can_read = 1;
			can_write = 1;
			can_execute = 1;
		}
	}

	if(is_valid == false)
	{
		
		// kill the process?
//		spinlock_release(tlb_splock);
		return EFAULT; // this is what dumbvm returns.
	
	}

	//if it reaches here, then the page is valid
	//check if permissions are valid
	if((faulttype == VM_FAULT_WRITE && can_write == 0) ||
		(faulttype == VM_FAULT_READONLY && can_write == 0) ||
		(faulttype == VM_FAULT_READ && can_read == 0))
	{
		//invalid operation on this page. kill the process?
		return EFAULT;
	}

	// can read though. so simply fix the tlb entry. what will the hardware do now?
	if(faulttype == VM_FAULT_READONLY && can_write == 1)
	{
		// fix the dirty bit of the tlb entry.
		uint32_t ehi, elo;	
		int spl = splhigh();
		int i;

		spinlock_acquire(tlb_splock);
		for (i=0; i<NUM_TLB; i++) 
		{
			tlb_read(&ehi, &elo, i);
			if (elo & TLBLO_VALID) 
			{
				if(ehi == faultaddress)
				{
					elo = elo | TLBLO_DIRTY; // set the dirty bit.
					tlb_write(ehi, elo, i);
					splx(spl);
					spinlock_release(tlb_splock);
					return 0;
				}
			}
		}
		//this should not get hit at all.
		KASSERT(i >= NUM_TLB);
	}

	
	// check if it is a page fault.
	bool in_page_table = false;
	paddr_t paddr = 0;
	struct list_node* temp = as->as_page_list;

	while(temp != NULL)
	{
		struct page_table_entry* page = (struct page_table_entry*)temp->node;
		if(page == NULL)
		{
		 	in_page_table = true;
		
		}
		KASSERT(page != NULL);

		if(page->vaddr == faultaddress)
		{
			in_page_table = true;
			paddr = page->paddr;
			// check if anything went horribly wrong.
	//		KASSERT(can_read == page->can_read);
		//	KASSERT(can_write == page->can_write);
	//		KASSERT(can_execute == page->can_execute);
			break;
		}
		temp = temp->next;
	
	}


	if(in_page_table == true)
	{
		//simple case. just update the tlb entry.
		// I already have the tlb lock.

		KASSERT(paddr != 0);

		uint32_t elo,ehi;
		int	spl = splhigh();

		spinlock_acquire(tlb_splock);
		for (int i=0; i < NUM_TLB; i++) 
		{
			tlb_read(&ehi, &elo, i);
			if (elo & TLBLO_VALID) 
			{
				continue;
			}
			
			ehi = faultaddress;
			elo = paddr | TLBLO_VALID;
		
			if(can_write == 1)
				elo = elo | TLBLO_DIRTY;

			tlb_write(ehi, elo, i);
			splx(spl);
			spinlock_release(tlb_splock);
			return 0;
		}

		//if i reach here, that means the tlb is full. what do i do now?.
		// tlb  full. use random.
		ehi = faultaddress;
		elo = paddr | TLBLO_VALID;

		if(can_write == 1)
			elo = elo | TLBLO_DIRTY;

		tlb_random(ehi, elo);
		splx(spl);
		spinlock_release(tlb_splock);
		return 0;
	}

	// Now to the fun part, TLB_FAULT and PAGE_FAULT
    // create memory for this page	

	void* v= kmalloc(PAGE_SIZE);
	if(v == NULL)	
		return ENOMEM;
		
	paddr = KVADDR_TO_PADDR((vaddr_t)v);
	as_zero_region(paddr,1);
	// create a page table entry
	struct page_table_entry* p = (struct page_table_entry*)kmalloc(sizeof(struct page_table_entry));
	p->vaddr = faultaddress;
	p->paddr = paddr;
//	p->can_read = can_read;
//	p->can_write = can_write;
//	p->can_execute = can_execute;

	// add this to the page table
	int i = add_node(&as->as_page_list, p);
	if(i == -1)
		return ENOMEM;
	
	// now add this to the TLB.
	uint32_t elo,ehi;
	int spl = splhigh();


	spinlock_acquire(tlb_splock);
	for (int i=0; i < NUM_TLB; i++) 
	{
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) 
		{
			continue;
		}
			
		ehi = faultaddress;
		elo = paddr | TLBLO_VALID;
		
		if(can_write == 1)
			elo = elo | TLBLO_DIRTY;

		tlb_write(ehi, elo, i);
		splx(spl);
		spinlock_release(tlb_splock);
		return 0;
	}

	//if i reach here, that means the tlb is full. what do i do now?.
	// tlb  full. use random.
	ehi = faultaddress;
	elo = paddr | TLBLO_VALID;

	if(can_write == 1)
		elo = elo | TLBLO_DIRTY;

	tlb_random(ehi, elo);
	splx(spl);
	spinlock_release(tlb_splock);
	(void)is_stack_page;
	(void)is_heap_page;
	(void)can_execute;
	return 0;


}


