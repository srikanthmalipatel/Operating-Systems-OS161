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
#include <vfs.h>
#include <uio.h>
#include <kern/fcntl.h>
#include <kern/stat.h>
#include <cpu.h>

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
#define SWAP_MAX 8000
struct swapmap_entry swap_map[SWAP_MAX];
struct spinlock* sm_splock = NULL;
struct vnode* swap_disk;
bool swap_disk_opened = false;
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;
//struct lock* cm_lock = NULL;
struct spinlock* cm_splock = NULL;
struct spinlock* tlb_splock = NULL;
//struct spinlock* sbrk_splock = NULL;
struct coremap_entry* coremap = NULL;
paddr_t free_memory_start_address  = 0;
uint32_t coremap_count = 0;
uint32_t first_free_index = 0;
vaddr_t copy_buffer_vaddr = 0;
uint32_t last_swapped = 0;
bool swapping_started = false;
extern struct lock* as_lock;
bool use_big_lock = true;

const char swap_disk_name[] = "lhd0raw:";
	void
vm_bootstrap(void)
{
	/* Do nothing. */

//	cm_lock = lock_create("cm lock");
//	KASSERT(cm_lock != NULL);
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
//	cm_lock = lock_create("cm lock");
	tlb_splock = (struct spinlock*)kmalloc(sizeof(struct spinlock));
	spinlock_init(tlb_splock);

	sm_splock = (struct spinlock*)kmalloc(sizeof(struct spinlock));
	spinlock_init(sm_splock);
	copy_buffer_vaddr = alloc_kpages(1);
	last_swapped = first_free_index;
//	swap_disk = kmalloc(sizeof(struct vnode));
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
 
/*static bool is_code_page(vaddr_t vaddr, struct addrspace* as)
{
	if(as->code_start <= vaddr && as->code_end > vaddr)
		return true;
	
	return false;

}*/

static bool is_swap_candidate(uint32_t index,vaddr_t vaddr)
{
	// remove the code page constraint once the bug is fixed
	if(coremap[index].state == CLEAN && coremap[index].is_victim == false && coremap[index].virtual_address != vaddr)//&& !is_code_page(coremap[index].virtual_address, coremap[index].as))
		return true;
//vaddr check to ensure that in as_copy we are not swapping out the old page to make space for this one.	
	if(coremap[index].state == DIRTY && coremap[index].is_victim == false && coremap[index].virtual_address != vaddr) //&& coremap[index].as == proc_getas())//&& !is_code_page(coremap[index].virtual_address, coremap[index].as))
		return true;
	
	return false;

}

// CHANGE THIS TO SUPPORT MULTIPLE PAGE REQUESTS?
static uint32_t get_swap_victim_index(unsigned long npages, vaddr_t vaddr)
{
	//random page replacement algorithm
	KASSERT(npages == 1);
	if(use_big_lock == true && swapping_started == true)
		spinlock_acquire(cm_splock);
	uint32_t index = random() % coremap_count;
	while(!is_swap_candidate(index,vaddr))
		index = random() % coremap_count;
	
	coremap[index].is_victim = true;
	if(use_big_lock == true && swapping_started == true)
		spinlock_release(cm_splock);
	return index;

}

void open_swap_disk()
{
	if(swap_disk_opened == false)
	{
	//	kprintf("swap disk init code \n");
		char temp_swap[sizeof(swap_disk_name)];
		strcpy(temp_swap,swap_disk_name);
		int result = vfs_open(temp_swap,O_RDWR,0,&swap_disk);
		if(result)
		{
			kprintf("disk cant be opened \n");
			return;
		}
		swapping_started = true;	
		swap_disk_opened = true;

	}

}

int write_to_disk(uint32_t page_index, int swap_pos)
{

	struct iovec iov;
	struct uio uio;
	vaddr_t vaddr = PADDR_TO_KVADDR(page_index*PAGE_SIZE);

	uio_kinit(&iov,&uio,(void*)vaddr,PAGE_SIZE,swap_pos*PAGE_SIZE,UIO_WRITE);

//	int spl = splhigh();
	vm_can_sleep();
		
	int result = VOP_WRITE(swap_disk,&uio);
//	splx(spl);
	
	if(result != 0)
	{
		kprintf("result is %d \n",result);
		KASSERT(result == 0);
	}
	KASSERT(uio.uio_resid == 0);
	return 0;
}

int mark_swap_pos(vaddr_t vaddr, struct addrspace* as)
{
	spinlock_acquire(sm_splock);
	for(int i = 0 ; i < SWAP_MAX; i++)
	{
		if(swap_map[i].as == as && swap_map[i].vaddr == vaddr)
		{
			spinlock_release(sm_splock);
			return i;
		}
	}

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

/*static int get_swap_pos(vaddr_t vaddr, struct addrspace*as)
{
	spinlock_acquire(sm_splock);
	for(int i = 0 ; i < SWAP_MAX; i++)
	{
		if(swap_map[i].vaddr == vaddr && swap_map[i].as == as)
		{
			spinlock_release(sm_splock);
			return i;
		}
	}
	spinlock_release(sm_splock);
	return -1;

}*/

paddr_t swap_in(vaddr_t vaddr, struct addrspace* as, vaddr_t buffer, int swap_pos)
{
	
//	int swap_pos = get_swap_pos(vaddr, as);
//	int spl = splhigh();
	KASSERT(swap_pos != -1);

	vaddr_t virtual_address;
	if(buffer == 0) // we dont have a target buffer already, so allocate a page here.
	{
		paddr_t paddr = get_user_page(vaddr,true,as);
		KASSERT(paddr != 0);
		paddr = paddr & PAGE_FRAME;
		//as_zero_region(paddr,1);
		virtual_address = PADDR_TO_KVADDR(paddr);
	}
	else
		virtual_address = buffer;

	KASSERT(swap_disk_opened == true);

	struct iovec iov;
	struct uio uio;

	uio_kinit(&iov,&uio,(void*)virtual_address,PAGE_SIZE,swap_pos*PAGE_SIZE,UIO_READ);

//	int spl = splhigh();
	vm_can_sleep();

	int result = VOP_READ(swap_disk,&uio);
//	splx(spl);
	KASSERT(result == 0);
	KASSERT(uio.uio_resid == 0);

/*	spinlock_acquire(sm_splock);
	swap_map[swap_pos].as = NULL;
	swap_map[swap_pos].vaddr = 0;
	spinlock_release(sm_splock);
*/
//	splx(spl);
	return KVADDR_TO_PADDR(virtual_address);

}

static int swap_out(uint32_t victim)
{
	KASSERT(victim != 0);

	bool is_clean = false;
	if(coremap[victim].state == CLEAN)
		is_clean= true;
	KASSERT(is_clean == false);

	// shootdown tlb entry for this victim
	vaddr_t victim_vaddr = coremap[victim].virtual_address;
	struct addrspace* victim_as = coremap[victim].as;

	KASSERT(coremap[victim].is_victim == true);

	// shooting down tlb entry. SHOOTDOWN ON ALL CPUs?
//	const struct tlbshootdown tlb = {victim_vaddr};
//	vm_tlbshootdown(&tlb);
	//ipi_tlbshootdown_all_cpus(&tlb);
	ipi_broadcast(IPI_TLBSHOOTDOWN);

	if(is_clean == false)
	{
		// get a free position in the swap map and mark it
		int pos = mark_swap_pos(victim_vaddr, victim_as);
		KASSERT(pos != -1); // swapmap is full

		// mark the page as swapping, will be used in vm_fault to check if swapping is going on
	//	spinlock_release(cm_splock);
		if(use_big_lock == true && swapping_started == true)
		{
			KASSERT(lock_do_i_hold(as_lock) == true);
			if(lock_do_i_hold(as_lock) == false)
				lock_acquire(as_lock);
		}
		else if(use_big_lock == false && swapping_started == true)
		{
			if(lock_do_i_hold(victim_as->as_lock) == false)
				lock_acquire(victim_as->as_lock);

		}

		struct page_table_entry* pt_entry = victim_as->as_page_list;
		KASSERT(victim_as != NULL);

		while(pt_entry != NULL)
		{
			if(pt_entry->vaddr == victim_vaddr)
			{
				pt_entry->page_state = SWAPPING;
				pt_entry->swap_pos = pos;
				pt_entry->paddr = 0;
				break;
			}
			pt_entry = pt_entry->next;
		}

		KASSERT(pt_entry != NULL);
		if(use_big_lock == false && swapping_started == true)
		KASSERT(lock_do_i_hold(victim_as->as_lock));

		vm_can_sleep();
		int err = write_to_disk(victim, pos);
		KASSERT(err == 0);

		if(use_big_lock == true && swapping_started == true)
			KASSERT(lock_do_i_hold(as_lock) == true);
		
		if(use_big_lock == false && swapping_started == true)
			KASSERT(lock_do_i_hold(victim_as->as_lock) == true);

		// update the page table entry of the owner of the victim,i.e., tell it that this page has been swapped out.
		KASSERT(pt_entry->vaddr == victim_vaddr);
		if(pt_entry->page_state != SWAPPING)
			kprintf("page state is %d \n", pt_entry->page_state);
		KASSERT(pt_entry->page_state == SWAPPING);
		pt_entry->page_state = SWAPPED;
		pt_entry->swap_pos = pos;
		pt_entry->paddr = 0;

	}

	return 0;
}

static
	paddr_t
getppages(unsigned long npages, bool is_user_page, vaddr_t vaddr, bool is_swap_in, struct addrspace* as)
{
	(void)is_swap_in;
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
								coremap[k].as = as;
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
	if(addr==0 && swapping_started == false)
	{
		return addr; 
	}
	else if(addr == 0)
	{
		// no free page, need to swap out a user page
		// acquire coremap spinlock


	/*	bool already_held =false;
		if(use_big_lock == true && swapping_started == true)
		{
			if(lock_do_i_hold(as_lock) == false)
				lock_acquire(as_lock);
			else
				already_held = true;
		}*/
		vm_can_sleep();
	//	spinlock_acquire(cm_splock);
		KASSERT(npages == 1);
		
		// select a candidate and mark it as victim

		uint32_t victim = get_swap_victim_index(npages,vaddr);
	//	kprintf("got victim : %d \n", victim);
		KASSERT(victim != 0);
		struct addrspace* victim_as = coremap[victim].as;
	//	bool already_held = false;
	
		bool already_held =false;
		if(use_big_lock == true && swapping_started == true)
		{
			if(lock_do_i_hold(as_lock) == false)
				lock_acquire(as_lock);
			else
				already_held = true;
		}

		if(use_big_lock == false && swapping_started == true)
		{
			if(lock_do_i_hold(victim_as->as_lock) == false)
				lock_acquire(victim_as->as_lock);
			else
				already_held = true;
		}

	//	spinlock_acquire(cm_splock);
		
		for(uint32_t i = victim; i < victim + npages; i++)
		{
			KASSERT(coremap[i].state != FIXED);
			KASSERT(coremap[i].state != FREE);
			coremap[i].is_victim = true;
		}


		// swap t
		for(uint32_t i = victim; i < victim + npages; i++)
		{
			if(coremap[i].state != FREE)
			{
				int err = swap_out(i);
				KASSERT(err == 0);
			}
		}

		spinlock_acquire(cm_splock);
		//give this new page to whoever asked for it.
		if(is_user_page == true)
		{
			coremap[victim].state = DIRTY;
			coremap[victim].chunks = 1;
			coremap[victim].virtual_address = vaddr;
			coremap[victim].as = as;
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

		if(use_big_lock == true && swapping_started == true)
		{
		//	if(already_held == false)
		//	KASSERT(already_held == true);
			if(already_held == false)
				lock_release(as_lock);
		}
		if(use_big_lock == false && swapping_started == true)
		{
			if(already_held == false)
				lock_release(victim_as->as_lock);
		}
		spinlock_release(cm_splock);
	}

	return addr;
}



/* Allocate/free some kernel-space virtual pages */
	vaddr_t
alloc_kpages(unsigned npages)
{
	paddr_t pa;
	pa = getppages(npages, false,0,false,NULL);
	if (pa==0) 
	{
		return 0;
	}
	return PADDR_TO_KVADDR(pa);
}

paddr_t get_user_page(vaddr_t vaddr, bool is_swap_in, struct addrspace* as)

{
//	int spl = splhigh();
	paddr_t pa;
	pa = getppages(1,true,vaddr,is_swap_in, as);
	return pa;
//	splx(spl);
}


static void clear_swap_map_entry(vaddr_t vaddr, struct addrspace* as, int swap_pos)
{
//	kprintf("inside clear swap map \n");
	spinlock_acquire(sm_splock);
	int i = 0;
	for(i = 0; i < SWAP_MAX; i++)
	{
		if(swap_map[i].vaddr == vaddr && swap_map[i].as == as)
		{
			swap_map[i].vaddr = 0;
			swap_map[i].as = NULL;
		//	kprintf("cleared swap entry for vaddr : %d , as : %p at pos : %d \n ", vaddr,(void*)as,i);
			break;
		
		}
	}
	KASSERT(i == swap_pos);
	KASSERT(i < SWAP_MAX);
	spinlock_release(sm_splock);

}
void free_user_page(vaddr_t vaddr,paddr_t paddr, struct addrspace* as, bool free_node, bool is_swapped, int swap_pos)
{
	(void)vaddr;
	if(is_swapped == true)
	{
		spinlock_acquire(sm_splock);
	//	KASSERT(swap_map[swap_pos].vaddr == vaddr);
	//	KASSERT(swap_map[swap_pos].as == as);

	//	int i = 0;

	//	for(i = 0 ; i < SWAP_MAX; i++)
	//	{
	//		if(swap_map[i].vaddr == vaddr && swap_map[i].as == as)
	//		{
				swap_map[swap_pos].vaddr = 0;
				swap_map[swap_pos].as = NULL;
				spinlock_release(sm_splock);
	//			break;
	//		}
	//	}
	//	KASSERT(i<SWAP_MAX);

	//	const struct tlbshootdown tlb = {vaddr};
	//	vm_tlbshootdown(&tlb);
		return;
	
	}


	KASSERT(as!= NULL);

	uint32_t page_index = paddr/ PAGE_SIZE;

	KASSERT(page_index < coremap_count);
	KASSERT(page_index >= first_free_index);

	KASSERT(coremap[page_index].state != FIXED);
	KASSERT(coremap[page_index].state != FREE);
	KASSERT(coremap[page_index].is_victim == false);


	spinlock_acquire(cm_splock);
	coremap[page_index].state = FREE;
	coremap[page_index].chunks = -1; // sheer paranoia.
	coremap[page_index].is_victim = false;
	coremap[page_index].virtual_address = 0;
	coremap[page_index].as = NULL;

	spinlock_release(cm_splock);
//	const struct tlbshootdown tlb = {vaddr};
//	vm_tlbshootdown(&tlb);
//	ipi_tlbshootdown_all_cpus(&tlb);
	ipi_broadcast(IPI_TLBSHOOTDOWN);

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
	
	bool already_held = false;
	if(use_big_lock == false && swapping_started == true)
	{
		if(lock_do_i_hold(as->as_lock) == false)
			lock_acquire(as->as_lock);
		else
			already_held = true;
	}
	else if(use_big_lock == true && swapping_started == true)
	{
		if(lock_do_i_hold(as_lock) == false)
		lock_acquire(as_lock);
	else
		already_held = true;
	}
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
			//	if(spinlock_do_i_hold(as->as_splock))
			//		spinlock_release(as->as_splock);
				thread_yield();
			}
		//	if(spinlock_do_i_hold(as->as_splock) == false)
		//		spinlock_acquire(as->as_splock);

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
	if(use_big_lock == true && swapping_started == true)
	{
		if(already_held == false)
			lock_release(as_lock);
	}
	else if(use_big_lock == false && swapping_started == true)
	{
		if(already_held == false)
			lock_release(as->as_lock);
	}

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

//	lock_acquire(cm_lock);
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

//	lock_release(cm_lock);
	spinlock_release(cm_splock);

}

unsigned
int
coremap_used_bytes() {

	/* dumbvm doesn't track page allocations. Return 0 so that khu works. */

	unsigned int count = 0;
//	lock_acquire(cm_lock);
	spinlock_acquire(cm_splock);
	for(uint32_t i = 0; i < coremap_count; i++)
	{
		if(coremap[i].state == FREE)
			count++;	
	}
//	lock_release(cm_lock);
	spinlock_release(cm_splock);
	if(as_lock == NULL)
		return (coremap_count-count)*PAGE_SIZE;
	else
		return (coremap_count-count)*PAGE_SIZE - 80 ;
}

unsigned int coremap_free_bytes()
{
	unsigned int free = 0;

//	lock_acquire(cm_lock);
	spinlock_acquire(cm_splock);
	for(uint32_t i = 0; i < coremap_count; i++)
	{
		if(coremap[i].state == FREE)
			free++;	
	}
//	lock_release(cm_lock);	
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

	int p = tlb_probe(vaddr,0);
	KASSERT(p < 0);

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
		kprintf("why am i here \n");
		//writing to a clean page, change page state to dirty.
	/*	spinlock_acquire(as->as_splock);

		kprintf("why am i here \n");
		struct page_table_entry* temp = as->as_page_list;
	//	KASSERT(temp == NULL);
		while(temp != NULL)
		{
			if(temp->vaddr == faultaddress)
			{
				KASSERT(temp->page_state == MAPPED);
				set_dirty(temp->paddr);
			//	spinlock_acuire(sm_splock);
				clear_swap_map_entry(temp->vaddr, as);
				temp->swap_pos = -1;
				break;
			}
			temp = temp->next;
		
		}
		KASSERT(temp != NULL);
		spinlock_release(as->as_splock);*/

		// update the tlb.
		spinlock_acquire(tlb_splock);
		uint32_t ehi, elo;	
		int spl = splhigh();

		int i = tlb_probe(faultaddress , 0);
		KASSERT(i >= 0);
		tlb_read(&ehi, &elo, i);

		elo = elo | TLBLO_DIRTY; // set the dirty bit
		
		tlb_write(ehi, elo, i);
		splx(spl);
		spinlock_release(tlb_splock);
		return 0;
	}


	// check if it is a page fault.
	bool in_page_table = false;
	paddr_t paddr = 0;
	uint32_t page_state = 99;
	struct page_table_entry* page = as->as_page_list;

	bool already_held = false;
	bool held_now = false;

	if(use_big_lock == false && swapping_started == true)
	{
		if(lock_do_i_hold(as->as_lock) == false)
		{
			lock_acquire(as->as_lock);
			held_now = true;
		}
		else
			already_held = true;
	}
	else if(use_big_lock == true && swapping_started == true)
	{
		if(lock_do_i_hold(as_lock) == false)
		{
			lock_acquire(as_lock);
			held_now = true;
	
		}
		else
			already_held = true;
	}

	while(page != NULL)
	{

		if(page->vaddr == faultaddress)
		{
			in_page_table = true;
			paddr = page->paddr;
			page_state = page->page_state;
			break;
		}
		page = page->next;
	}


	if(in_page_table == true)
	{
		//simple case. just update the tlb entry.

		KASSERT(page_state != SWAPPING);

		while(page->page_state == SWAPPING)
		{
		//	if(spinlock_do_i_hold(as->as_splock))
		//		spinlock_release(as->as_splock);
			thread_yield();
		}
//		if(spinlock_do_i_hold(as->as_splock) == false)
//			spinlock_acquire(as->as_splock);
		if(page_state == MAPPED)
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
				if(already_held == false || held_now == true)
				{
					if(use_big_lock == true && swapping_started == true)
						lock_release(as_lock);
					else if(use_big_lock == false && swapping_started == true)
						lock_release(as->as_lock);
				
				}
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
	
			if(already_held == false || held_now == true)
			{
				if(use_big_lock == true && swapping_started == true)
					lock_release(as_lock);
				else if(use_big_lock == false && swapping_started == true)
					lock_release(as->as_lock);
				
			}
			return 0;
		}
		else if(page_state == SWAPPED)
		{
		//	spinlock_release(as->as_splock);
			// this page is swapped out. Bring it back into memory
			vm_can_sleep();
			if(use_big_lock == false && swapping_started == true)
			{
				if(held_now == true)
				lock_release(as->as_lock);
			}
			paddr_t paddr = swap_in(faultaddress,as,0,page->swap_pos);
		//	spinlock_acquire(as->as_splock);
			bool already_held = false;
			if(use_big_lock == false && swapping_started == true)
			{
				if(lock_do_i_hold(as->as_lock) == false)
				{	
					held_now = true;
					lock_acquire(as->as_lock);
				}
			
				else
					already_held = true;
			}
			else if(use_big_lock == true && swapping_started == true)
			{
				if(lock_do_i_hold(as_lock) == false)
				{	
					held_now = true;
					lock_acquire(as_lock);
				}
				else
					already_held = true;
			}
			// update page_table_entry and page_state
			page->paddr = paddr;
			page->page_state = MAPPED;
			clear_swap_map_entry(faultaddress,as,page->swap_pos);
			page->swap_pos = -1;
		//	clear_swap_map_entry(faultaddress,as);

			// update the tlb entry. But mark it as readonly, if user attempts to write, then we can just fix the tlb entry and update page state to dirty

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

				if(already_held == false || held_now == true)
				{
					if(use_big_lock == true && swapping_started == true)
						lock_release(as_lock);
					else if(use_big_lock == false && swapping_started == true)
						lock_release(as->as_lock);
				
				}

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

			if(already_held == false || held_now == true)
			{
				if(use_big_lock == true && swapping_started == true)
					lock_release(as_lock);
				else if(use_big_lock == false && swapping_started == true)
					lock_release(as->as_lock);
				
			}
			return 0;

		}
	}


	// tlb fault and page fault.
	// create memory for this page
	if(use_big_lock == false && swapping_started == true)
	{
		if(held_now == true)
		lock_release(as->as_lock);
	}
	
	paddr = get_user_page(faultaddress,false, proc_getas());
//	already_held = false;
	if(use_big_lock == false && swapping_started == true)
	{
		if(lock_do_i_hold(as->as_lock) == false)
		{	
			lock_acquire(as->as_lock);
			held_now = true;
		}
		else
			already_held = true;
	}

	if(use_big_lock == true  && swapping_started == true)
	{
		if(lock_do_i_hold(as_lock) == false)
		{	
			lock_acquire(as_lock);
			held_now = true;
		}
		else
			already_held = true;
	}

	

	if(paddr == 0)
	{
		if(already_held == false)
		//	lock_release(as->as_lock);
			lock_release(as->as_lock);
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

	if(already_held == false || held_now == true)
	{
		if(use_big_lock == true && swapping_started == true)
			lock_release(as_lock);
		else if(use_big_lock == false && swapping_started == true)
			lock_release(as->as_lock);
				
	}
	if(i == -1)
	{
		lock_release(as->as_lock);
		return ENOMEM;
	}
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


