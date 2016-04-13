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
#include <filesystem.h>
#include <signal.h>

int sys__exit(int exitcode) {
    struct proc* proc = curproc;
    KASSERT(proc != NULL);
    //kprintf("[sys_exit] Process[%d] Entering exit\n", curproc->pid);
    // first we need to exit the current thread. So that it moves to zombie state and will be cleaned up.
    //kprintf("[sys__exit] Exiting thread - %s\n", curthread->t_name);
        file_table_cleanup(curproc->t_file_table);
        proc->exited = true;
        if(exitcode == SIGSEGV || exitcode == SIGTRAP || exitcode == SIGILL || exitcode == SIGFPE)
        	proc->exitcode = _MKWAIT_SIG(exitcode);
        else
        	proc->exitcode = _MKWAIT_EXIT(exitcode);
        thread_exit();

    return 0;
}
