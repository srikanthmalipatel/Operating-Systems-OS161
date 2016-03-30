#include <kern/sys_sbrk.h>
#include <addrspace.h>

int sys_sbrk(intptr_t amount, int* retval)
{
	struct addrspace* as = proc_getas();
	if(as == NULL)
		return ENOMEM;
	
	vaddr_t heap_start = as->as_heap_start;
	vaddr_t heap_end = as->as_heap_end;


	if(amount == 0)
	{
		*retval = heap_end;
		return 0;
	
	}

	// unaligned , return inval.
	if(amount > 0 && amount != ROUNDUP(amount,4))
	{
		*retval = -1;
		return EINVAL;
	
	}

	// asking for too much
	if(amount > 0 && heap_end - VM_STACKBOUND + amount > 0) // essentially checking if heap_end + amount > VM_STACKBOUND, doing it like this to avoid overflows.
	{
		*retval = -1;
		return ENOMEM;
	
	}

	// trying to corrupt, return inval
	if(heap_end + amount < heap_start)
	{
		*retval = -1;
		return EINVAL;
	}
	


	amount = ROUNDUP(amount,4); // suggestion in jinghao's blog, should find out why this is done.

	*retval = heap_end;
	as->as_heap_end += amount;
	return 0;

}
