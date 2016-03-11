/*
 * sys_execv.h
 * Copyright (C) 2016 SrikanthMalipatel <SrikanthMalipatel@Srikanth>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef SYS_EXECV_H
#define SYS_EXECV_H

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

int sys_execv(userptr_t progname, userptr_t *arguments);

#endif /* !SYS_EXECV_H */
