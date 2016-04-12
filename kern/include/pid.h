/*
 * pid.h
 * Copyright (C) 2016 SrikanthMalipatel <SrikanthMalipatel@Srikanth>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef PID_H
#define PID_H

#include<kern/limits.h>

#define __MAX_PROC 32 

struct procManager {
    struct semaphore* p_sem;                    // updating file table must be an atomic and mutually exclusive
    struct proc *p_table[__MAX_PROC];                   // process table
};

struct procManager* init_pid_manager(void);
void destroy_pid_manager(struct procManager *);
pid_t alloc_pid(struct proc*);
int dealloc_pid(struct proc*);
struct proc* get_proc_pid(pid_t pid);
bool check_ppid_exists(struct proc*);
#endif /* !PID_H */
