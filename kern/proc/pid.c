#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <array.h>
#include <proc.h>
#include <synch.h>


// remove kassets in case of memory alloc failure
struct procManager* init_pid_manager() {
    struct procManager *pm;
    pm = (struct procManager *) kmalloc(sizeof(struct procManager));
    KASSERT(pm != NULL);
    pm->p_lock = lock_create("proc lock");
    KASSERT(pm->p_lock != NULL);
    int i;
    for(i=0; i<__PID_MAX; i++) {
        pm->p_table[i] = NULL;
    }
    return pm;
}

pid_t alloc_pid() {
    return 0;
}
