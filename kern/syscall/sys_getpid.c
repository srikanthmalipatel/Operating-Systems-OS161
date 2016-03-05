#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <syscall.h>
#include <test.h>
#include <kern/sys_getpid.h>

int sys_getpid(int *retval) {
    *retval = curproc->pid;
    return 0;
}
