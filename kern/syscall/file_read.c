
#include <file_read.h>
#include <syscall.h>
#include <types.h>
#include <copyinout.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <vfs.h>
#include <current.h>


int sys_read(int fd, userptr_t buf, int len)
{


	(void)fd;
	(void)buf;
	(void) len;

	return 0;








}
