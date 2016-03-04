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
    int i;
    lock_acquire(&pm->plock);
	for(i=2; i<__PID_MAX; i++) {
		if(pm->p_table[i] == NULL)
			return i; 
	}    
    lock_release(&pm->plock);
    return ENPROC;
}
