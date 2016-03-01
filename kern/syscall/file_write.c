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
	(void)fd;
	(void)buf;
	(void)nBytes;
	(void)retval;

	return 0;


}
