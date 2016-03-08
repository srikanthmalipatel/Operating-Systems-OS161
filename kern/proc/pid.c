#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <array.h>
#include <proc.h>
#include <synch.h>

/*
 *  TODO: we need to synchronize operations on p_table while allocating and deallocating pid's
 */
// remove kassets in case of memory alloc failure
struct procManager* init_pid_manager() {
    struct procManager *pm;
    pm = (struct procManager *) kmalloc(sizeof(struct procManager));
    if(pm == NULL)
        return NULL;
    pm->p_lock = lock_create("proc lock");
    if(pm->p_lock == NULL) {
        kfree(pm);
        return NULL;
    }
    int i;
    for(i=0; i<__MAX_PROC; i++) {
        pm->p_table[i] = NULL;
    }
    return pm;
}

void destroy_pid_manager(struct procManager *pm) {
    KASSERT(pm != NULL);
    //lock_acquire(pm->p_lock);
    int i;
    for(i=0; i<__MAX_PROC; i++) {
        pm->p_table[i] = NULL;
    }
    //lock_release(pm->p_lock);
    //lock_destroy(pm->p_lock);
    kfree(pm);
    return;
}


// first process will be kernel thread
// second process is the process created by run program
pid_t alloc_pid(struct proc *userproc) {
    int i;
    //lock_acquire(p_manager->p_lock);
	for(i=2; i<__MAX_PROC; i++) {
		if(p_manager->p_table[i] == NULL)
            p_manager->p_table[i] = userproc;
            //lock_release(p_manager->p_lock);
			return i; 
    }
    //lock_release(p_manager->p_lock);
    return -1;
}

int dealloc_pid(struct proc *userproc) {
    int i;
    //lock_acquire(p_manager->p_lock);
    for(i=2; i<__MAX_PROC; i++) {
        if(i == userproc->pid) {
            p_manager->p_table[i] = NULL;
            //lock_release(p_manager->p_lock);
            return 1;
        }
    }
    //lock_release(p_manager->p_lock);
    return 0;
}

bool check_ppid_exists(struct proc *userproc) {
    int i;
    //lock_acquire(p_manager->p_lock);
    for(i=2; i<__MAX_PROC; i++) {
        if(i == userproc->ppid) {
            //lock_release(p_manager->p_lock);
            return true;
        }
    }
    //lock_release(p_manager->p_lock);
    return false;
}
