/*
 * sys_fork.h
 * Copyright (C) 2016 SrikanthMalipatel <SrikanthMalipatel@Srikanth>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef SYS_FORK_H
#define SYS_FORK_H

#include <types.h>
//struct trapframe;
// nead to change prototype
int sys_fork(struct trapframe* tf, int* retval); 

#endif /* !SYS_FORK_H */
