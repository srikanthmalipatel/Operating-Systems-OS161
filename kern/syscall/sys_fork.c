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
#include <vnode.h>

int sys_fork(struct trapframe* tf, int* retval) {
    struct proc *newproc;
    int result;

    // copy the parents trapframe into kernel heap 
    //kprintf("[sys_fork] creating trapframe\n");
    struct trapframe* childtf = (struct trapframe*) kmalloc(sizeof(struct trapframe));
    if(childtf == NULL) {
        return ENOMEM;
    }
    *childtf = *tf;
    
    // create process
    //kprintf("[sys_fork] creating process\n");
    newproc = proc_create("user process");
    if (newproc == NULL) {
        kfree(childtf);
        return ENOMEM;
    }
    
    // allocate pid and assign ppid
    newproc->pid = alloc_pid(newproc);
    if (newproc->pid == -1) {
        kfree(childtf);
        proc_destroy(newproc);
        // too many processes in the system
        return ENPROC;
    }
    newproc->ppid = curproc->pid;

	spinlock_acquire(&curproc->p_lock);
	if (curproc->p_cwd != NULL) {
		VOP_INCREF(curproc->p_cwd);
		newproc->p_cwd = curproc->p_cwd;
	}
	spinlock_release(&curproc->p_lock);
    // copy the parent address space
    struct addrspace* c_addrspace;
    //kprintf("[sys_fork] copying address space\n");
    result = as_copy(curproc->p_addrspace, &c_addrspace); 
    if (result) {
    //	as_destroy(c_addrspace);
        kfree(childtf);
        dealloc_pid(newproc);
        proc_destroy(newproc);
        return result;
    }

    //kprintf("[sys_fork] copying filetable space\n");
    file_table_copy(curproc->t_file_table, newproc->t_file_table);

    // create a child thread and pass trapframe and address space
    //kprintf("[sys_fork] creating new thread\n");
    result = thread_fork("user process",
            newproc,
            enter_forked_process,
            (void *) childtf, (unsigned long) c_addrspace);
    if (result) {
        as_destroy(c_addrspace);
        kfree(childtf);
        dealloc_pid(newproc);
        proc_destroy(newproc);
        return ENOMEM;
    }

    // return child pid to parent
    *retval = newproc->pid; 
    return 0;
}
