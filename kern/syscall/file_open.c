#include <file_open.h>
#include <syscall.h>
#include <types.h>
#include <copyinout.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <vfs.h>
#include <current.h>
#include <kern/stat.h>
#include <proc.h>
int sys_open(const_userptr_t filename, int flags, int mode, int* retval)
{

	// same process calls open multiple times on the same file, what do we do ??
	//wiki says its ok to have multiple file handles to the same file in the same process' file table.

	//should we be bothered about concurrency when it comes to the actual reading and writing of the vnode (not the handle)


	
	// !!!!!!!!!!  how do we handle the additional crap that we OR with the flags, like create, append etc.
	// write a separate function to check all cases , but instead of writing a huge if condition, we can make use of the position of the bits in tohe int passed.
	// for example. only one of the 3 least significant bits should be set. and the bit corresponding to O_EXCL should be set only if the bit for o_CREAT is set.

	if(!is_valid_flag(flags)) // as mentioned above, write a function to check for this.
	{
		return EINVAL;
	}
	
	if(filename == NULL)
	{
		return EFAULT;
	}

	int fd = get_free_file_descriptor(curproc->t_file_table);
	if(fd == -1)
		return EMFILE;
	
/*	size_t len = strlen((const char*)filename); // culprit !! 
	if(len > NAME_MAX)
		return EFAULT ; // just in case, but is EFAULT the correct error response;
*/
	int result;
	size_t* actual = NULL;
	char kern_file_name[NAME_MAX + 1];
	memset(kern_file_name, 0, NAME_MAX + 1);


	// are we using this properly?.. check jinghao's blog for example
	result = copyinstr(filename, kern_file_name, NAME_MAX + 1, actual); // using this because users are stupid/malicious and can pass invalid memory addresses to the kernel.
	if(result)
	{
		return result;

	}

	char kern_file_name_copy[NAME_MAX + 1]; // using this because kern_file_name gets modified inside vfs_open.
	memset(kern_file_name_copy , 0, NAME_MAX + 1);
	strcpy(kern_file_name_copy, kern_file_name);
	
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
	
	strcpy(fh->file_name,kern_file_name_copy); // safe to use because we are playing with only the kernel memory;

	if(is_append(flags))
	{
		struct stat file_stats;
		result = VOP_STAT(filenode, &file_stats);
		if(result)
			return result;
		fh->offset = file_stats.st_size;
	}
	else
		fh->offset = 0; // check if O_APPEND is passed, if yes, then use VOP_STAT to get the file size and update offset with the file size.
	
	curproc->t_file_table[fd] = fh;

	return 0;

}
