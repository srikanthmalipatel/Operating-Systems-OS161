
#include <file_read.h>
#include <syscall.h>
#include <types.h>
#include <copyinout.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <vfs.h>
#include <current.h>
#include <proc.h>
#include <uio.h>
int sys_read(int fd, userptr_t buf, int len, int* retval)
{
	// argument checks.

	if(fd <0 || fd>= OPEN_MAX)
		return EBADF;
	
	int result;
	char kern_buffer[len + 1];

	// are we using this properly?.. check jinghao's blog for example
	// no actual use for the kern buffer, just doing this to check if memory location is valid.
	result = copyin(buf, kern_buffer, len); // using this because users are stupid/malicious and can pass invalid memory addresses to the kernel.
	if(result)
	{
		return result;

	}

	struct file_handle* fh = get_file_handle(curproc->t_file_table, fd);
	
	if(fh == NULL)
		return EBADF;

	if(!can_read(fh->openflags)) // could have other flags Or'd with O_RDONLY, need to change this.
		return EBADF;
	

// !!!! Should we do copyin's to kernel space, or will the VOP_WRITE take care of the invalid address issue for us.

	lock_acquire(fh->fh_lock); // IS this really necessary??.. turns out it is , offset should be synchronized. imagine if parent and child call this at the same time.
	struct iovec iov;
	struct uio u;

	iov.iov_ubase = (userptr_t)buf;
	iov.iov_len = len;		 // length of the memory space
	u.uio_iov = &iov;
	u.uio_iovcnt = 1;
	u.uio_resid = len;          // amount to read from the file
	u.uio_offset = fh->offset;
	u.uio_segflg =  UIO_USERSPACE;
	u.uio_rw = UIO_READ;
	u.uio_space = proc_getas(); // lifted from loadelf.c, is this the right way to do it?

	result = VOP_READ(fh->file, &u);
	if (result) {
		lock_release(fh->fh_lock);
		return result;
	}

	if (u.uio_resid != 0) {
		kprintf("ELF: short read on segment - file truncated?\n");
	}

	// should update offset in the file handle.use lock. uio_offset will be updated. can use it directly.
	fh->offset = u.uio_offset;

	lock_release(fh->fh_lock);

	*retval = len - u.uio_resid; // number of bytes gets returned to the user
	return 0; 


}
