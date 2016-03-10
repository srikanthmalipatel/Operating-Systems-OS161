#ifndef SYS_WAIT_H
#define SYS_WAIT_H

#include <types.h>

int sys_waitpid(pid_t pid, userptr_t status, int options, int *retval);
 
#endif /* !SYS_WAIT_H */
