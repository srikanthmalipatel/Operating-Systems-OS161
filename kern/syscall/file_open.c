#include <file_open.h>
#include <syscall.h>
#include <types.h>
#include <copyinout.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <vfs.h>
#include <current.h>
//#include <filesystem.h>

int sys_open(const_userptr_t filename, int flags, int mode, int* retval)
{

	// same process calls open multiple times on the same file, what do we do ??
	//wiki says its ok to have multiple file handles to the same file.

	//should we be bothered about concurrency when it comes to the actual reading and writing of the vnode (not the handle)


	if(flags != O_RDONLY && flags != O_WRONLY && flags != O_RDWR)
	{
		return EINVAL;
	}
	
	// !!!!!!!!!!  how do we handle the additional crap that we OR with the flags, like create, append etc.?
	if(filename == NULL)
	{
		return EFAULT;
	}

	int fd = get_free_file_descriptor(curthread->t_file_table);
	if(fd == -1)
		return EMFILE;
	
	size_t len = strlen((const char*)filename);
	if(len > NAME_MAX)
		return EFAULT ; // just in case, but is EFAULT the correct error response;

	int result;
	size_t* actual = NULL;
	char kern_file_name[len + 1];


	// are we using this properly?.. check jinghao's blog for example
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

	*retval = fd; // this file descriptor will be returned to the user

	struct file_handle* fh = file_handle_create();
	fh->openflags = flags;
	fh->ref_count = 1;
	fh->file = *filenode;
	
	strcpy(fh->file_name,kern_file_name); // safe to use because we are playing with only the kernel memory;

	fh->offset = 0; // check if O_APPEND is passed, if yes, then use VOP_STAT to get the file size and update offset with the file size.
	
	curthread->t_file_table[fd] = fh;

	return 0;

}
