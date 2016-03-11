
#include <file_getcwd.h>
#include <syscall.h>
#include <types.h>
#include <copyinout.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <vfs.h>
#include <current.h>
#include <kern/stat.h>
#include <uio.h>
#include <proc.h>
int sys_getcwd(userptr_t buf, size_t len)
{
	if(buf == NULL || len > PATH_MAX)
		return EFAULT;

	int result;
	char kern_buffer[len + 1];

	// are we using this properly?.. check jinghao's blog for example
	// no actual use for the kern buffer, just doing this to check if memory location is valid.
	result = copyin(buf, kern_buffer, len); // using this because users are stupid/malicious and can pass invalid memory addresses to the kernel.
	if(result)
	{
		return result;

	}

	struct iovec iov;
	struct uio u;

	iov.iov_ubase = (userptr_t)buf;
	iov.iov_len = len;		 // length of the memory space
	u.uio_iov = &iov;
	u.uio_iovcnt = 1;
	u.uio_resid = len;          // amount to read from the file
	u.uio_offset = 0;
	u.uio_segflg =  UIO_USERSPACE;
	u.uio_rw = UIO_READ;
	u.uio_space = proc_getas(); // lifted from loadelf.c, is this the right way to do it?

	result = vfs_getcwd( &u);
	if (result) {
		return result;
	}

	return 0;



}
