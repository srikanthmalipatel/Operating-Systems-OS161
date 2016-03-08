#include <kern/sys_exit.h>
#include <syscall.h>
#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <current.h>
#include <proc.h>
#include <pid.h>
#include <kern/wait.h>

int sys__exit(int exitcode) {
    struct proc* proc = curproc;
    KASSERT(proc != NULL);

    // first we need to exit the current thread. So that it moves to zombie state and will be cleaned up.
    //kprintf("[sys__exit] Exiting thread - %s\n", curthread->t_name);
    thread_exit();
    //if(check_ppid_exists(proc)) {
        proc->exited = true;
        proc->exitcode = _MKWAIT_EXIT(exitcode);
        V(proc->p_exitsem);
    /*} else {
        dealloc_pid(proc);
        proc_destroy(proc);
    }*/
    return 0;
}
