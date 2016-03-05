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

pid_t sys_getpid() {
    return curproc->pid;
}
