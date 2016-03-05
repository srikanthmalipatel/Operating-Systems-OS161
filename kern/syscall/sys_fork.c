#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <syscall.h>
#include <test.h>
#include <mips/trapframe.h>
#include <kern/sys_fork.h> /* Definintion for sys_fork() */

static void enter_child_fork(void* ptr, unsigned long args) {
    struct trapframe childtf;
    bzero(&childtf, sizeof(childtf));
    childtf = *(struct trapframe *)ptr;
    childtf.tf_v0 = 0;
    childtf.tf_a3 = 0;
    childtf.tf_epc += 4;
    // load and activate the address space
    struct addrspace* as = (struct addrspace *)args;
    proc_setas(as);
    as_activate();

    mips_usermode(&childtf);
}

int sys_fork(struct trapframe* tf, int* retval) {
    struct proc *newproc;
    int result;

    // copy the parents trapframe into kernel heap 
    struct trapframe* childtf = (struct trapframe*) kmalloc(sizeof(struct trapframe));
    *childtf = *tf;
    
    // create process
    newproc = proc_create_runprogram("user process");
    if (newproc == NULL) {
        return ENOMEM;
    }
    
    // allocate pid and assign ppid
    newproc->pid = alloc_pid(newproc);
    if (newproc->pid == -1) {
        proc_destroy(newproc);
        // too many processes in the system
        return ENPROC;
    }
    newproc->ppid = curproc->pid;

    // copy the parent address space
    result = as_copy(curproc->p_addrspace, &newproc->p_addrspace); 
    if (result) {
        proc_destroy(newproc);
        dealloc_pid(newproc);
        return result;
    }

    // create a child thread and pass trapframe and address space
    result = thread_fork_proc("proc thread",
            newproc,
            enter_child_fork,
            (void *) childtf, (unsigned long) newproc->p_addrspace);
    if (result) {
        proc_destroy(newproc);
        dealloc_pid(newproc);
        proc_destroy(newproc);
        return result;
    }

    // return child pid to parent
    *retval = newproc->pid; 
    return 0;
}
