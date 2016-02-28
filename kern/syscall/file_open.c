#include <file_open.h>
#include <syscall.h>
#include <types.h>
#include <copyinout.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <vfs.h>
#include <current.h>

int sys_open(const_userptr_t filename, int flags, int mode, int* retval)
{
	if(flags != O_RDONLY && flags != O_WRONLY && flags != O_RDWR)
	{
		return EINVAL;
	}
	
	// how do we handle the additional crap that we OR with the flags, like create, append etc.?
	if(filename == NULL)
	{
		return EFAULT;
	}
	//check if there is space in the process' file table. if yes proceed, else return EMFILE


	int fd = get_free_file_descriptor(curthread->t_file_table);
	if(fd == -1)
		return EMFILE;
	
	size_t len = strlen((const char*)filename);
	int result;
	size_t* actual = NULL;
	char* kern_file_name = NULL;

	result = copyinstr(filename, kern_file_name, len, actual);
	if(result)
	{
		return result;

	}


	struct vnode** filenode = NULL;
	int err =  vfs_open(kern_file_name, flags, mode, filenode);	
	if(err)
	{
		return err;
	}

	retval = fd; // this file descriptor will be returned to the user

// retval will store the file descriptor.
	(void)retval;


	// get next file descriptor
	//create a new file handle, add the correspoding offset and filenode fields and the reference count 
	//and put it in the filetable of the current thread at the file descriptor position

	//return the file descriptor



	return -1;




}
