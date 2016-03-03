#include <file_dup2.h>
#include <file_close.h>
#include <syscall.h>
#include <types.h>
#include <copyinout.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <vfs.h>
#include <current.h>

int sys_dup2(int oldfd, int newfd, int* retval)
{
	if(oldfd < 0 || oldfd >= OPEN_MAX)
		return EBADF;
	
	if(newfd <0 || newfd >= OPEN_MAX)
		return EBADF;
	
	struct file_handle* fh = get_file_handle(curthread->t_file_table, oldfd);
	
	if(fh == NULL)
		return EBADF;

	lock_acquire(fh->fh_lock); // what if another process is doing something else on the same file handle while it is trying to move it.

	struct file_handle* fh1 = get_file_handle(curthread->t_file_table, newfd);
	if(fh1 == NULL)
	{
		// directly copy
		curthread->t_file_table[newfd] = fh;
		fh->ref_count += 1;
	
	}
	else
	{
		// call close on the new file;
		sys_close(newfd);
		curthread->t_file_table[newfd] = fh;
		fh->ref_count += 1;
	
	
	}
	
	lock_release(fh->fh_lock);
	*retval = newfd;
	return 0;




}
