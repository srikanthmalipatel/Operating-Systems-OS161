#include <kern/sys_sbrk.h>
#include <addrspace.h>
#include <mips/tlb.h>

extern struct spinlock* sbrk_splock;
extern struct spinlock* tlb_splock;

int sys_sbrk(intptr_t amount, int* retval)
{
#if OPT_DUMBVM
    (void) amount;
    (void) retval;
    return 0;
#else

	spinlock_acquire(sbrk_splock);
	struct addrspace* as = proc_getas();
	if(as == NULL)
	{
		spinlock_release(sbrk_splock);
		return ENOMEM;
	}
	intptr_t heap_start = as->as_heap_start;
	intptr_t heap_end = as->as_heap_end;


	if(amount == 0)
	{
		*retval = heap_end;
		spinlock_release(sbrk_splock);
		return 0;
	
	}

	unsigned int s = coremap_free_bytes();
	(void)s;
	unsigned int p  = amount;
	// should probably change this when swapping is implemented.
	if(amount > 0 && p > coremap_free_bytes())
	{
		*retval = -1;
		
		spinlock_release(sbrk_splock);
		return ENOMEM;
	}

	// unaligned , return inval.
	if(amount > 0 && (amount!= ROUNDUP(amount,PAGE_SIZE)))
	{
		*retval = -1;

		spinlock_release(sbrk_splock);
		return EINVAL;
	
	}

	// asking for too much
	size_t request = heap_end + amount;
	size_t bound = VM_STACKBOUND;
	if(amount > 0 && request > bound) // essentially checking if heap_end + amount > VM_STACKBOUND, doing it like this to avoid overflows.
	{
		*retval = -1;

		spinlock_release(sbrk_splock);
		return ENOMEM;
	
	}

	// trying to corrupt, return inval
	if(heap_end + amount < heap_start)
	{
		*retval = -1;

		spinlock_release(sbrk_splock);
		return EINVAL;
	}

	if(amount < 0)
 		free_heap(amount);

	*retval = heap_end;
	as->as_heap_end += amount;
	spinlock_release(sbrk_splock);
	return 0;
#endif

}
