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
    pm->p_sem = sem_create("pid sem",0);
    if(pm->p_sem == NULL) {
        kfree(pm);
        panic("lock create failed");
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
    V(pm->p_sem);
    int i;
    for(i=0; i<__MAX_PROC; i++) {
        pm->p_table[i] = NULL;
    }
    P(pm->p_sem);
    sem_destroy(pm->p_sem);
    //lock_release(pm->p_lock);
    //lock_destroy(pm->p_lock);
    kfree(pm);
    return;
}


// first process will be kernel thread
// second process is the process created by run program
pid_t alloc_pid(struct proc *userproc) {
    int i, result = -1;
    //lock_acquire(p_manager->p_lock);
    V(p_manager->p_sem);
	for(i=2; i<__MAX_PROC; i++) {
		if(p_manager->p_table[i] == NULL) {
            p_manager->p_table[i] = userproc;
            //lock_release(p_manager->p_lock);
            result = i;
            break;
        }
    }
    P(p_manager->p_sem);
    //lock_release(p_manager->p_lock);
    return result;
}

int dealloc_pid(struct proc *userproc) {
    int i, result = -1;
    //lock_acquire(p_manager->p_lock);
    V(p_manager->p_sem);
    for(i=2; i<__MAX_PROC; i++) {
        if(i == userproc->pid) {
            //kprintf("[dealloc_pid] found pid - %d\n", i);
            p_manager->p_table[i] = NULL;
            //lock_release(p_manager->p_lock);
            result = i;
            break;
        }
    }
    P(p_manager->p_sem);
    //lock_release(p_manager->p_lock);
    return result;
}

struct proc* get_proc_pid(pid_t pid) {
    int i;
    struct proc *result = NULL;
    V(p_manager->p_sem);
    for(i=2; i<__MAX_PROC; i++) {
        if(i == pid) {
            result = p_manager->p_table[i];
            break;
        }
    }
    P(p_manager->p_sem);
    return result;
}

bool check_ppid_exists(struct proc *userproc) {
    //kprintf("[check_ppid] checking for ppid - %d \n", userproc->ppid);
    bool exists = false;
    int i;
    if(userproc->ppid == -1)
        return false;
    //lock_acquire(p_manager->p_lock);
    V(p_manager->p_sem);
    for(i=2; i<__MAX_PROC; i++) {
        //kprintf("[check_ppid] %d\n", i);
        if(i == userproc->ppid) {
            //kprintf("[check_ppid] found ppid \n");
            //lock_release(p_manager->p_lock);
            exists = true;
            break;
        }
    }
    P(p_manager->p_sem);
    //lock_release(p_manager->p_lock);
    return exists;
}
