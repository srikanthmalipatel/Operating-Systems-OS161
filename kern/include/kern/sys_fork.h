/*
 * sys_fork.h
 * Copyright (C) 2016 SrikanthMalipatel <SrikanthMalipatel@Srikanth>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef SYS_FORK_H
#define SYS_FORK_H
//struct trapframe;
// nead to change prototype
int sys_fork(struct trapframe *tf); 

#endif /* !SYS_FORK_H */
