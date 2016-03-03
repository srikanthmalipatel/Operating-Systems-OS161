/*
 * pid.h
 * Copyright (C) 2016 SrikanthMalipatel <SrikanthMalipatel@Srikanth>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef PID_H
#define PID_H

#include<kern/limits.h>

struct procManager {
    struct lock *p_lock;                    // updating file table must be an atomic and mutually exclusive
    struct proc *p_table[__PID_MAX];                   // process table
};

struct procManager* init_pid_manager(void);
pid_t alloc_pid(void);
void dealloc_pid(pid_t pid);
#endif /* !PID_H */