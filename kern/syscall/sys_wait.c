#include <kern/sys_wait.h>
#include <syscall.h>
#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <current.h>
#include <copyinout.h>
#include <proc.h>
#include <pid.h>
#include <kern/wait.h>

int sys_waitpid(pid_t pid, userptr_t status, int options, int *retval) {
    (void) options;
    int result;

    /* Arguments Error Handling */

    struct proc* waitproc = get_proc_pid(pid);
    if(waitproc == NULL) {
        return ESRCH;
    }
    // check if status pointer is aligned by 4 bytes
    if((uintptr_t)(const void *)(status) % 4 != 0) {
        return EFAULT;
    }

    // check for badflags
    if(options != 0) {
        return EINVAL;
    }

    if(pid == curthread->t_proc->pid || pid == curthread->t_proc->ppid) {
        return ECHILD;
    }

    if(curthread->t_proc->pid != waitproc->ppid) {
        return ECHILD;
    }
    
    if(waitproc->exited == false)
        P(waitproc->p_exitsem);
    if((int *) status != NULL) {
        result = copyout((const void *) &(waitproc->exitcode), status, sizeof(int));
        if(result)
            //V(waitproc->p_exitsem);
            return EFAULT;
    }
    dealloc_pid(waitproc);
    proc_destroy(waitproc);
    *retval = pid;
    return 0;
}
