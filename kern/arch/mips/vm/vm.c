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
#include <thread.h>

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
#define SWAP_MAX 2000
struct swapmap_entry swap_map[SWAP_MAX];
struct spinlock* sm_splock = NULL;
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;
struct spinlock* cm_splock = NULL;
struct spinlock* tlb_splock = NULL;
//struct spinlock* sbrk_splock = NULL;
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
		coremap[i].is_victim = false;

	}

	for(int i = 0 ; i < SWAP_MAX; i++)
	{
		swap_map[i].vaddr = 0;
		swap_map[i].as = NULL;
	}

	vm_initialized = true;
	tlb_splock = (struct spinlock*)kmalloc(sizeof(struct spinlock));
	spinlock_init(tlb_splock);

	sm_splock = (struct spinlock*)kmalloc(sizeof(struct spinlock));
	spinlock_init(sm_splock);
}

/*
 * Check if we're in a context that can sleep. While most of the
 * operations in dumbvm don't in fact sleep, in a real VM system many
 * of them would. In those, assert that sleeping is ok. This helps
 * avoid the situation where syscall-layer code that works ok with
 * dumbvm starts blowing up during the VM assignment.
 */

 static
 void
 vm_can_sleep(void)
 {
 	if (CURCPU_EXISTS()) 
  	{
		// must not hold spinlocks 
		KASSERT(curcpu->c_spinlocks == 0);

		//must not be in an interrupt handler 
		KASSERT(curthread->t_in_interrupt == 0);
	}
}
 

// CHANGE THIS TO SUPPORT MULTIPLE PAGE REQUESTS
static uint32_t get_swap_victim_index(unsigned long npages)
{
	if(npages == 1)
	{
		paddr_t dirty_address = 0;

		for(uint32_t i = first_free_index ; i < coremap_count; i++)
		{
			if(coremap[i].state == CLEAN && coremap[i].is_victim == false)
				return i;

			if(dirty_address == 0 && coremap[i].state == DIRTY && coremap[i].is_victim == false)
				dirty_address = i;
		}

		return dirty_address;
	}
	else
	{
		// kernel asking for multiple pages,
		// it needs contiguous pages.
		// one option is give it any random sequence of n contiguous pages (user pages and free)
		// another option is to iterate and find out an optimal sequence which requires the least number of swap outs.
		
		// this is the worst possible solution. should optimize this function
		for(uint32_t i = first_free_index; i < coremap_count; i++)
		{
			if((coremap[i].is_victim == false) && (coremap[i].state == FREE || coremap[i].state == CLEAN || coremap[i].state == DIRTY))
			{
				unsigned long p = 0;
				uint32_t j = i + 1;
				while((p < (npages - 1)) && j < coremap_count)
				{
					if((coremap[j].state == FREE || coremap[j].state == CLEAN || coremap[j].state == DIRTY)&&(coremap[j].is_victim == false))
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
					return i;
				}
			
			}
			
		}
	
	}

	return -1;
}
static int write_to_disk(uint32_t page_index, int swap_pos)
{
	(void)page_index;
	(void)swap_pos;


	return 0;
}

static int mark_swap_pos(vaddr_t vaddr, struct addrspace* as)
{
	spinlock_acquire(sm_splock);

	for(int i = 0 ; i < SWAP_MAX; i++)
	{
		if(swap_map[i].as == NULL)
		{
			KASSERT(swap_map[i].vaddr == 0);
			swap_map[i].vaddr = vaddr;
			swap_map[i].as = as;
			spinlock_release(sm_splock);
			return i;

		}

	}
	spinlock_release(sm_splock);
	return -1;
}

static int swap_out(uint32_t victim)
{
	KASSERT(victim != 0);

	bool is_clean = false;
	if(coremap[victim].state == CLEAN)
		is_clean= true;

	// shootdown tlb entry for this victim, also we need to do something to the page table entry to mark it from being used.
	vaddr_t victim_vaddr = coremap[victim].virtual_address;
	struct addrspace* victim_as = coremap[victim].as;

	// shooting down tlb entry. SHOOTDOWN ON ALL CPU's?
	const struct tlbshootdown tlb = {victim_vaddr};
	//	vm_tlbshootdown(&tlb);
	ipi_tlbshootdown_broadcast(&tlb);

	if(is_clean == false)
	{
		// get a free position in the swap map and mark it
		int pos = mark_swap_pos(victim_vaddr, victim_as);
		KASSERT(pos != -1); // swapmap is full

		// mark the page as swapping, will be used in vm_fault to check if swapping is going on
		spinlock_acquire(victim_as->as_splock);
		struct page_table_entry* pt_entry = victim_as->as_page_list;
		KASSERT(victim_as != NULL);

		while(pt_entry != NULL)
		{
			if(pt_entry->vaddr == victim_vaddr)
			{
				pt_entry->page_state = SWAPPING;
				pt_entry->swap_pos = pos;
				break;
			}
			pt_entry = pt_entry->next;
		}

		KASSERT(pt_entry != NULL);
		spinlock_release(victim_as->as_splock);

		// release the cm_spinlock
		spinlock_release(cm_splock);

		// check if we can sleep
		vm_can_sleep();
		// do the actual swap out to the position.
		int err = write_to_disk(victim, pos);
		KASSERT(err == 0);


		// update the page table entry of the owner of the victim,i.e., tell it that this page has been swapped out.
		spinlock_acquire(victim_as->as_splock);
		KASSERT(pt_entry->vaddr == victim_vaddr);
		KASSERT(pt_entry->page_state == SWAPPING);
		pt_entry->page_state = SWAPPED;

		spinlock_release(victim_as->as_splock);
		spinlock_acquire(cm_splock);
	}
	else
	{
		// already a clean page, so just mark it as swapped.
		spinlock_acquire(victim_as->as_splock);
		struct page_table_entry* pt_entry = victim_as->as_page_list;
		KASSERT(victim_as != NULL);

		while(pt_entry != NULL)
		{
			if(pt_entry->vaddr == victim_vaddr)
			{
				KASSERT(pt_entry->page_state == MAPPED);
				pt_entry->page_state = SWAPPED;
				break;
			}
			pt_entry = pt_entry->next;
		}

		KASSERT(pt_entry != NULL);
		spinlock_release(victim_as->as_splock);
	}

	return 0;
}

static
	paddr_t
getppages(unsigned long npages, bool is_user_page, vaddr_t vaddr)
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
		if(first_free_index + npages >= coremap_count)
			addr = 0;
		else
		{
			spinlock_acquire(cm_splock);
			for(uint32_t i = first_free_index; i < coremap_count; i++)
			{
				if(coremap[i].state == FREE)
				{
					KASSERT(coremap[i].is_victim == false);
					unsigned long p = 0;
					uint32_t j = i + 1;
					while((p < (npages - 1)) && j < coremap_count)
					{
						if(coremap[j].state == FREE)
						{
							KASSERT(coremap[j].is_victim == false);
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
						coremap[i].chunks = npages;
						for(unsigned long k = i; k < i+npages; k++)
						{
							// !!! Dirty should be set only when called from page_alloc. basically means that we need to update the disk.
							// When called from kmalloc, the page can never be swapped. so it should be FIXED/PINNED.

							// No. kernel threads can call kmalloc, set owner as current process. So can be dirty, just need to check if it is kernel to set FIXED.
							if(is_user_page == true)
							{
								coremap[k].state = DIRTY;
								coremap[k].virtual_address = vaddr;
								coremap[k].as = proc_getas();
								coremap[k].is_victim = false;

							}
							else
							{
								coremap[k].state = FIXED;
								coremap[k].virtual_address = 0;
								coremap[k].as = NULL;
								coremap[k].is_victim = false;
							}
						}
						break;
					}
				}
			}
			spinlock_release(cm_splock);
		}

	}

	// swapping code
	// This is different for user and kernel pages. Kernel pages require contiguous swapping
	// TO DO : Support for kernel contiguous pages.
	is_user_page = false;
	if(is_user_page == true && addr == 0)
	{
		// no free page, need to swap out a user page
		// acquire coremap spinlock
		spinlock_acquire(cm_splock);

		// select a candidate and mark it as victim
		uint32_t victim = get_swap_victim_index(npages);
		KASSERT(victim != 0);

		for(uint32_t i = victim; i < victim + npages; i++)
		{
			KASSERT(coremap[i].state != FIXED);
			coremap[i].is_victim = true;
		}

		for(uint32_t i = victim; i < victim + npages; i++)
		{
			if(coremap[i].state != FREE)
			{
				int err = swap_out(i);
				KASSERT(err == 0);
			}
		}



		//give this new page to whoever asked for it.
		if(is_user_page == true)
		{
			coremap[victim].state = DIRTY;
			coremap[victim].chunks = 1;
			coremap[victim].virtual_address = vaddr;
			coremap[victim].as = proc_getas();
			coremap[victim].is_victim = false;
		}
		else
		{
			coremap[victim].state = FIXED;
			coremap[victim].chunks = npages;
			coremap[victim].virtual_address = 0;
			coremap[victim].as = NULL;
			coremap[victim].is_victim = false;
			for(uint32_t i = victim + 1 ; i < victim + npages; i++)
			{
				coremap[i].state = FIXED;
				coremap[i].chunks = -1;
				coremap[i].virtual_address = 0;
				coremap[i].as = NULL;
				coremap[i].is_victim = false;
			}
		}

		addr = victim*PAGE_SIZE;
		spinlock_release(cm_splock);
	}

	return addr;
}



/* Allocate/free some kernel-space virtual pages */
	vaddr_t
alloc_kpages(unsigned npages)
{
	paddr_t pa;

	pa = getppages(npages, false,0);
	if (pa==0) 
	{
		return 0;
	}
	return PADDR_TO_KVADDR(pa);
}

paddr_t get_user_page(vaddr_t vaddr)
{
	paddr_t pa;
	pa = getppages(1,true,vaddr);

	return pa;
}

void free_user_page(vaddr_t vaddr,paddr_t paddr, struct addrspace* as, bool free_node, bool is_swapped, int swap_pos)
{
	KASSERT(as!= NULL);

	uint32_t page_index = paddr/ PAGE_SIZE;

	KASSERT(page_index < coremap_count);
	KASSERT(page_index >= first_free_index);

	KASSERT(coremap[page_index].state != FIXED);
	KASSERT(coremap[page_index].state != FREE);
	KASSERT(coremap[page_index].is_victim == false);


	if(is_swapped == true)
	{
		spinlock_acquire(sm_splock);
		KASSERT(swap_map[swap_pos].vaddr == vaddr);
		KASSERT(swap_map[swap_pos].as == as);

		swap_map[swap_pos].vaddr = 0;
		swap_map[swap_pos].as = NULL;
		spinlock_release(sm_splock);
		return;
	
	}

	spinlock_acquire(cm_splock);
	coremap[page_index].state = FREE;
	coremap[page_index].chunks = -1; // sheer paranoia.
	coremap[page_index].is_victim = false;
	coremap[page_index].virtual_address = 0;
	coremap[page_index].as = NULL;

	spinlock_release(cm_splock);
	const struct tlbshootdown tlb = {vaddr};
	vm_tlbshootdown(&tlb);
	//	ipi_tlbshootdown_broadcast(&tlb);


	if(free_node == true)
	{
		struct page_table_entry* temp = as->as_page_list;
		KASSERT(temp!= NULL);

		struct page_table_entry* prev = NULL;
		if(temp->paddr == page_index*PAGE_SIZE)
		{
			as->as_page_list = temp->next;	
			kfree(temp);

		}
		else
		{
			while(temp != NULL)
			{
				if(temp->paddr == page_index*PAGE_SIZE)
				{
					KASSERT(prev != NULL);
					prev->next = temp->next;
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
	}
}

void free_heap(intptr_t amount)
{
	struct addrspace* as = proc_getas();
	KASSERT(as!= NULL);
	KASSERT(amount < 0);

	spinlock_acquire(as->as_splock);
	intptr_t heap_end = as->as_heap_end;

	intptr_t chunk_start = as->as_heap_end + amount; //amount is -ve.

	struct page_table_entry* temp = as->as_page_list;
	struct page_table_entry* next = NULL;
	struct page_table_entry* prev = NULL;
	while(temp != NULL)
	{
		next = temp->next;
		intptr_t vaddr = temp->vaddr;
		if(vaddr >= chunk_start && vaddr < heap_end)
		{
			KASSERT(temp->page_state == MAPPED);
			while(temp->page_state == SWAPPING)
			{
				thread_yield();
			}

			bool is_swapped = false;
			if(temp->page_state == SWAPPED)
				is_swapped = true;
				
			free_user_page(temp->vaddr,temp->paddr,as,false, is_swapped, temp->swap_pos);
			prev->next = next;
			kfree(temp);
		}
		else
			prev = temp;

		temp = next;
	}
	spinlock_release(as->as_splock);

}

	void
free_kpages(vaddr_t addr)
{
	if(vm_initialized == false)
		return;


	paddr_t paddr = KVADDR_TO_PADDR(addr);	
	uint32_t page_index = paddr/ PAGE_SIZE;


	if(page_index > coremap_count)
		return;

	spinlock_acquire(cm_splock);
	KASSERT(coremap[page_index].state != FREE);
	KASSERT(coremap[page_index].is_victim == false);

	//Should we allow the user to do this !!.. IMPORTANT - ASK TA.
	//km2 and km5 are asking me to free memory that they have not called to be allocated. Is this fine ??
	if(page_index < first_free_index)
	{

		kprintf("*** freeing a page which was allocated with ram_stealmem() *****");
		coremap[page_index].state = FREE;
		coremap[page_index].chunks = -1;
		coremap[page_index].is_victim = false;

	}
	else
	{
		int chunks = coremap[page_index].chunks;
		while(chunks > 0 && (page_index >= first_free_index) && (page_index < coremap_count))
		{
			coremap[page_index].state = FREE;
			coremap[page_index].chunks = -1; // sheer paranoia.
			coremap[page_index].is_victim = false;
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
	panic("dumbvm1 tried to do tlb shootdown?!\n");
}

	void
vm_tlbshootdown(const struct tlbshootdown *ts)
{

	KASSERT(ts != NULL);
	vaddr_t vaddr = ts->vaddr;

	spinlock_acquire(tlb_splock);
	int	spl = splhigh();
	int index = tlb_probe(vaddr,0);

	if(index >= 0)
		tlb_write(TLBHI_INVALID(index), TLBLO_INVALID(), index);

	splx(spl);
	spinlock_release(tlb_splock);

}

	int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	if(faulttype != VM_FAULT_READ && faulttype != VM_FAULT_WRITE && faulttype != VM_FAULT_READONLY)
		return EINVAL;

	if(curproc == NULL)
		return EFAULT;

	struct addrspace *as = NULL;
	as = proc_getas();
	if(as == NULL)
		return EFAULT;

	//check if fault_address is valid.
	faultaddress &= PAGE_FRAME;

	bool is_valid = false;
	int can_read = 0, can_write = 0, can_execute = 0;
	bool is_stack_page = false, is_heap_page = false;


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



	// check the code and data regions
	struct as_region* temp = as->as_region_list;
	while(is_valid == false && temp != NULL)
	{
		vaddr_t vaddr = temp->region_base;
		size_t npages = temp->region_npages;

		if(faultaddress >= vaddr  && faultaddress < vaddr + PAGE_SIZE*npages)
		{
			is_valid = true;
			can_read = temp->can_read;
			can_write = temp->can_write;
			can_execute = temp->can_execute;

			break;
		}
		temp = temp->next;
	}

	//does not belong to any of the regions, not a valid address.
	if(is_valid == false)
	{
		return EFAULT;
	}

	//if it reaches here, then the page is valid
	//check if permissions are valid
	if((faulttype == VM_FAULT_WRITE && can_write == 0) ||
			(faulttype == VM_FAULT_READONLY && can_write == 0) ||
			(faulttype == VM_FAULT_READ && can_read == 0))
	{
		//invalid operation on this page. kill the process
		return EFAULT;
	}

	// can write though. so simply fix the tlb entry
	if(faulttype == VM_FAULT_READONLY && can_write == 1)
	{
		uint32_t ehi, elo;	
		int spl = splhigh();

		int i = tlb_probe(faultaddress , 0);
		KASSERT(i >= 0);
		tlb_read(&ehi, &elo, i);

		elo = elo | TLBLO_DIRTY; // set the dirty bit.
		tlb_write(ehi, elo, i);
		splx(spl);
		spinlock_release(tlb_splock);
		return 0;
	}


	// check if it is a page fault.
	bool in_page_table = false;
	bool is_swapped = false;
	paddr_t paddr = 0;
	struct page_table_entry* page = as->as_page_list;

	spinlock_acquire(as->as_splock);
	while(page != NULL)
	{

		if(page->vaddr == faultaddress)
		{
			in_page_table = true;
			paddr = page->paddr;
			if(page->page_state == SWAPPED)
				is_swapped = true;
			break;
		}
		page = page->next;
	}


	if(in_page_table == true)
	{
		//simple case. just update the tlb entry.

		if(is_swapped == false)
		{
			//direct case, just update the tlb entry.
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
				spinlock_release(as->as_splock);
				return 0;
			}

			// tlb  full. use random.
			ehi = faultaddress;
			elo = paddr | TLBLO_VALID;

			if(can_write == 1)
				elo = elo | TLBLO_DIRTY;

			tlb_random(ehi, elo);
			splx(spl);
			spinlock_release(tlb_splock);
			spinlock_release(as->as_splock);
			return 0;
		}
		else
		{
			// this page is swapped out. Bring it back into memory
			// update the page table with the new paddr and is_swapped = false
			// update the tlb entry.

		}
	}


	// tlb fault and page fault.
	// create memory for this page	
	paddr = get_user_page(faultaddress);
	if(paddr == 0)
	{
		spinlock_release(as->as_splock);
		return ENOMEM;
	}
	as_zero_region(paddr,1);

	//create a page table entry for this page.
	struct page_table_entry* p = (struct page_table_entry*)kmalloc(sizeof(struct page_table_entry));
	p->vaddr = faultaddress;
	p->paddr = paddr;
	p->page_state = MAPPED;
	p->swap_pos = -1;

	// add this to the page table
	int i = page_list_add_node(&as->as_page_list, p);
	spinlock_release(as->as_splock);
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


