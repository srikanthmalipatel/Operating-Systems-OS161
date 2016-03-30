#include <kern/sys_sbrk.h>
#include <addrspace.h>
int sys_sbrk(size_t amount, int* retval)
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
