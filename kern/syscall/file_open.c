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
	//wiki says its ok to have multiple file handles to the same file in the same process' file table.

	//should we be bothered about concurrency when it comes to the actual reading and writing of the vnode (not the handle)


	
	// !!!!!!!!!!  how do we handle the additional crap that we OR with the flags, like create, append etc.
	// write a separate function to check all cases , but instead of writing a huge if condition, we can make use of the position of the bits in tohe int passed.
	// for example. only one of the 3 least significant bits should be set. and the bit corresponding to O_EXCL should be set only if the bit for o_CREAT is set.

	if(flags != O_RDONLY && flags != O_WRONLY && flags != O_RDWR) // as mentioned above, write a function to check for this.
	{
		return EINVAL;
	}
	
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
	result = copyinstr(filename, kern_file_name, len + 1, actual); // using this because users are stupid/malicious and can pass invalid memory addresses to the kernel.
	if(result)
	{
		return result;

	}

	struct vnode* filenode;
	int err =  vfs_open(kern_file_name, flags, mode, &filenode);	// this is where the file is actually being opened
	if(err)
	{
		return err;
	}

	*retval = fd; // this file descriptor will be returned to the user

	struct file_handle* fh = file_handle_create();
	fh->openflags = flags;
	fh->ref_count = 1;
	fh->file = filenode;
	
	strcpy(fh->file_name,kern_file_name); // safe to use because we are playing with only the kernel memory;

	fh->offset = 0; // check if O_APPEND is passed, if yes, then use VOP_STAT to get the file size and update offset with the file size.
	
	curthread->t_file_table[fd] = fh;

	return 0;

}
