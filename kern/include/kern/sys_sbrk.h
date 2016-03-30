
#ifndef SYS_SBRK_H
#define SYS_SBRK_H

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <array.h>
#include <cpu.h>
#include <spl.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <threadlist.h>
#include <threadprivate.h>
#include <proc.h>
#include <current.h>
#include <synch.h>
#include <kern/fcntl.h>
#include <kern/sys_execv.h>

int sys_sbrk(size_t amount, int *retval);

#endif /* !SYS_SBRK_H */
