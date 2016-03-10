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
    struct proc* waitproc = get_proc_pid(pid);
    KASSERT(waitproc != NULL);
    P(waitproc->p_exitsem);
    result = copyout((const void *) &(waitproc->exitcode), status, sizeof(int));
    if(result)
        return EFAULT;
    dealloc_pid(waitproc);
    proc_destroy(waitproc);
    *retval = pid;
    return 0;
}
