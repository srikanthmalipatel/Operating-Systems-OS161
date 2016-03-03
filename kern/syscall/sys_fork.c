#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <syscall.h>
#include <test.h>
#include <kern/sys_fork.h> /* Definintion for sys_fork() */

int sys_fork(struct trapframe *tf) {
    // copy the parents trapframe into kernel heap and copy it
    //struct trapframe *childtf = (struct trapframe *) kmalloc(sizeof(struct trapframe));
    //*childtf = *tf;

    (void)tf;
    return 0;
}
