
#include <file_chdir.h>
#include <syscall.h>
#include <types.h>
#include <copyinout.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <vfs.h>
#include <current.h>
#include <kern/stat.h>

int sys_chdir(const userptr_t filename)
{
	
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

	result = vfs_chdir(kern_file_name);
	if(result)
		return result;

	return 0;
}
