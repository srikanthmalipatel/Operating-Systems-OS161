#include <file_write.h>
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

int sys_write(int fd, const userptr_t buf, size_t nBytes, int* retval)
{

	if(buf == NULL) // does VOP_WRITE take care of this for us.?
		return EFAULT;

// recitation slides ask us to use copyin/out to check if this pointer is valid, so i guess that VOP_WRITE does not do it for us.
	
	int result;
	char kern_buffer[nBytes + 1];

	// are we using this properly?.. check jinghao's blog for example
	// no actual use for the kern buffer, just doing this to check if memory location is valid.
	result = copyin(buf, kern_buffer, nBytes); // using this because users are stupid/malicious and can pass invalid memory addresses to the kernel.
	if(result)
	{
		return result;

	}


	if(fd < 0 || fd >= OPEN_MAX )
		return EBADF;
	
	struct file_handle* fh = get_file_handle(curthread->t_file_table, fd);
	if(fh == NULL)
		return EBADF;

	if(!can_write(fh->openflags)) // Again, this needs to be changed based on the appended flags.
		return EBADF;

// in the man page, it is mentioned that this operation should be atomic on the file and that the OS cannot make a guarantee.
// Does that mean that it is the user's responsibility to ensure that it is atomic.
// or should we do something to make sure that not more than one operation happens on the vnode?

 // !!!! Should we do copyin's to kernel space, or will the VOP_WRITE take care of the invalid address issue for us.

	lock_acquire(fh->fh_lock); // IS this really necessary??.. turns out it is , offset should be synchronized. imagine if parent and child call this at the same time.
	struct iovec iov;
	struct uio u;

	iov.iov_ubase = buf;
	iov.iov_len = nBytes;		   // length of the memory space
	u.uio_iov = &iov;
	u.uio_iovcnt = 1;
	u.uio_resid = nBytes;          // amount to write to the file
	u.uio_offset = fh->offset;
	u.uio_segflg =  UIO_USERSPACE;
	u.uio_rw = UIO_WRITE;
	u.uio_space = proc_getas();    // lifted from loadelf.c, is this the right way to do it?

	result = VOP_WRITE(fh->file, &u);
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

	*retval = nBytes - u.uio_resid; // number of bytes gets returned to the user
	return 0; 


}
