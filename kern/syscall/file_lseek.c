#include <file_lseek.h>
#include <syscall.h>
#include <types.h>
#include <copyinout.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <vfs.h>
#include <current.h>
#include <kern/stat.h>
#include <kern/seek.h>

off_t sys_lseek(int fd, off_t pos, int whence, off_t* retval)
{

	if(fd < 0 || fd >= OPEN_MAX)
		return EBADF;
	
	struct file_handle* fh = get_file_handle(curthread->t_file_table, fd);
	
	if(fh == NULL)
		return EBADF;

	lock_acquire(fh->fh_lock);

	if(whence == SEEK_SET)
		fh->offset = pos;
	
	else if(whence == SEEK_CUR)
		fh->offset += pos;
	
	else if(whence == SEEK_END)
	{
					
		struct stat file_stats;
		int result = VOP_STAT(fh->file, &file_stats);
		if(result)
			return result;
		fh->offset = file_stats.st_size + pos;
	
	
	}

	else
	{
		lock_release(fh->fh_lock);
		return EINVAL;
	
	}

	// should we handle integer overflow cases?

	*retval = fh->offset;
	lock_release(fh->fh_lock);

	return 0;

}
