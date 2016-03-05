
#include <file_close.h>
#include <syscall.h>
#include <types.h>
#include <copyinout.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <vfs.h>
#include <current.h>


int sys_close(int fd)
{
	// argument checks, 
	//is fd in range and is there a valid file handle in the file table at this fd

	if(fd < 0 || fd >= OPEN_MAX)
		return EBADF;
	
	struct file_handle* fh = get_file_handle(curthread->t_file_table, fd);
	if(fh == NULL)
		return EBADF;


	// So the fd is good, Now we work on the file handle, 
	// Call vfs_close on the vnode held by the file handle
	// reduce the refcount of this file handle by 1 , using a lock just in case.
	// if count is 0, destroy the file handle

	lock_acquire(fh->fh_lock);
	fh->ref_count--;
	lock_release(fh->fh_lock);
	
	if(fh->ref_count == 0)
	{
		
		vfs_close(fh->file);
		file_handle_destroy(fh);
	
	}

	// should set this to null even if the file handle is not destroyed.
	 curthread->t_file_table[fd] = NULL;

	return 0;


}
