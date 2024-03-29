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

#ifndef _VM_H_
#define _VM_H_

/*
 * VM system-related definitions.
 *
 * You'll probably want to add stuff here.
 */

/*#include<linked_list.h>
#include <addrspace.h>
typedef enum state
{
	FREE,
	FIXED,
	DIRTY,
	CLEAN

}page_state;


 struct coremap_entry
 {
//	vaddr_t virtual_address;
	page_state state;
    unsigned int chunks : 7;
//	struct addrspace* as;

 };
*/
#include <machine/vm.h>

#include<linked_list.h>
typedef enum state
{
	FREE,
	FIXED,
	DIRTY,
	CLEAN

}page_state;

#define FREE 0
#define FIXED 1
#define DIRTY 2
#define CLEAN 3
#define VICTIM 4
 struct coremap_entry
 {
	vaddr_t virtual_address;
	unsigned int state : 3;
    unsigned int chunks : 7;
	struct addrspace* as;
	unsigned int is_victim : 1;
	struct lock* page_lock;

 };

 struct swapmap_entry
 {
 	vaddr_t vaddr;
 	struct addrspace* as;
 
 };

struct addrspace* as;
/* Fault-type arguments to vm_fault() */
#define VM_FAULT_READ        0    /* A read was attempted */
#define VM_FAULT_WRITE       1    /* A write was attempted */
#define VM_FAULT_READONLY    2    /* A write to a readonly page was attempted*/


/* Initialization function */
void vm_bootstrap(void);

/* Fault handling function called by trap code */
int vm_fault(int faulttype, vaddr_t faultaddress);

/* Allocate/free kernel heap pages (called by kmalloc/kfree) */
vaddr_t alloc_kpages(unsigned npages);
void free_kpages(vaddr_t addr);//, bool, struct addrspace* as);

paddr_t get_user_page(vaddr_t, bool, struct addrspace*);
void free_user_page(vaddr_t vaddr,paddr_t paddr, struct addrspace* as, bool free_node, bool is_swapped, int swap_pos);
void free_heap(intptr_t amount);

/*
 * Return amount of memory (in bytes) used by allocated coremap pages.  If
 * there are ongoing allocations, this value could change after it is returned
 * to the caller. But it should have been correct at some point in time.
 */
unsigned int coremap_used_bytes(void);
void open_swap_disk(void);
void create_coremap_locks(void);

unsigned int coremap_free_bytes(void);
/* TLB shootdown handling called from interprocessor_interrupt */
void vm_tlbshootdown_all(void);
void vm_tlbshootdown(const struct tlbshootdown *);

paddr_t swap_in(vaddr_t vaddr, struct addrspace* as, vaddr_t buffer, int swap_pos);
int mark_swap_pos(vaddr_t vaddr, struct addrspace* as);

int write_to_disk(uint32_t page_index, int swap_pos);
#endif /* _VM_H_ */
