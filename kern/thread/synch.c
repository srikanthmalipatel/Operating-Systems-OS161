/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Synchronization primitives.
 * The specifications of the functions are in synch.h.
 */

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <current.h>
#include <synch.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *name, unsigned initial_count)
{
	struct semaphore *sem;

	sem = kmalloc(sizeof(*sem));
	if (sem == NULL) {
		return NULL;
	}

	sem->sem_name = kstrdup(name);
	if (sem->sem_name == NULL) {
		kfree(sem);
		return NULL;
	}

	sem->sem_wchan = wchan_create(sem->sem_name);
	if (sem->sem_wchan == NULL) {
		kfree(sem->sem_name);
		kfree(sem);
		return NULL;
	}

	spinlock_init(&sem->sem_lock);
	sem->sem_count = initial_count;

	return sem;
}

void
sem_destroy(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	/* wchan_cleanup will assert if anyone's waiting on it */
	spinlock_cleanup(&sem->sem_lock);
	wchan_destroy(sem->sem_wchan);
	kfree(sem->sem_name);
	kfree(sem);
}

void
P(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	/*
	 * May not block in an interrupt handler.
	 *
	 * For robustness, always check, even if we can actually
	 * complete the P without blocking.
	 */
	KASSERT(curthread->t_in_interrupt == false);

	/* Use the semaphore spinlock to protect the wchan as well. */
	spinlock_acquire(&sem->sem_lock);
	while (sem->sem_count == 0) {
		/*
		 *
		 * Note that we don't maintain strict FIFO ordering of
		 * threads going through the semaphore; that is, we
		 * might "get" it on the first try even if other
		 * threads are waiting. Apparently according to some
		 * textbooks semaphores must for some reason have
		 * strict ordering. Too bad. :-)
		 *
		 * Exercise: how would you implement strict FIFO
		 * ordering?
		 */
		wchan_sleep(sem->sem_wchan, &sem->sem_lock);
	}
	KASSERT(sem->sem_count > 0);
	sem->sem_count--;
	spinlock_release(&sem->sem_lock);
}

void
V(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	spinlock_acquire(&sem->sem_lock);

	sem->sem_count++;
	KASSERT(sem->sem_count > 0);
	wchan_wakeone(sem->sem_wchan, &sem->sem_lock);

	spinlock_release(&sem->sem_lock);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
	struct lock *lock;

	lock = kmalloc(sizeof(*lock));
	if (lock == NULL) {
		return NULL;
	}

	lock->lk_name = kstrdup(name);
	if (lock->lk_name == NULL) {
		kfree(lock);
		return NULL;
	}

	lock->lk_wchan = wchan_create(lock->lk_name);
	if (lock->lk_wchan == NULL) {
		kfree(lock->lk_name);
		kfree(lock);
		return NULL;
	}

	spinlock_init(&lock->lk_spinlock);
	lock->lk_isheld = false;
	lock->lk_curthread = NULL;
	// add stuff here as needed

	return lock;
}

void
lock_destroy(struct lock *lock)
{
	KASSERT(lock != NULL);
    KASSERT(lock_do_i_hold(lock) == false);
	// add stuff here as needed
    lock->lk_curthread = NULL;
	spinlock_cleanup(&lock->lk_spinlock);
	wchan_destroy(lock->lk_wchan);
	kfree(lock->lk_name);
	kfree(lock);
}

void
lock_acquire(struct lock *lock)
{
	// Write this
	if(lock == NULL)
    KASSERT(lock != NULL);
    KASSERT(curthread->t_in_interrupt == false);

    // current thread should not try to acquire the lock again
   // KASSERT(lock->lk_curthread != curthread);
	KASSERT(lock_do_i_hold(lock) == false);
    spinlock_acquire(&lock->lk_spinlock);
    while(lock->lk_isheld == true) {
        wchan_sleep(lock->lk_wchan, &lock->lk_spinlock);
    }
    KASSERT(lock->lk_isheld == false);
    lock->lk_isheld = true;
    lock->lk_curthread = curthread;
    spinlock_release(&lock->lk_spinlock);
	//(void)lock;  // suppress warning until code gets written
}

void
lock_release(struct lock *lock)
{
	// Write this
    KASSERT(lock != NULL);
    if(lock_do_i_hold(lock) == false)
    KASSERT(lock_do_i_hold(lock) == true);
    spinlock_acquire(&lock->lk_spinlock);
    lock->lk_isheld = false;
    lock->lk_curthread = NULL;
    wchan_wakeone(lock->lk_wchan, &lock->lk_spinlock);
    spinlock_release(&lock->lk_spinlock);
	//(void)lock;  // suppress warning until code gets written
}

bool
lock_do_i_hold(struct lock *lock)
{
	// Write this

	//(void)lock;  // suppress warning until code gets written

	return (lock->lk_curthread == curthread); // dummy until code gets written
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
	struct cv *cv;

	cv = kmalloc(sizeof(*cv));
	if (cv == NULL) {
		return NULL;
	}

	cv->cv_name = kstrdup(name);
	if (cv->cv_name==NULL) {
		kfree(cv);
		return NULL;
	}

	cv->cv_wchan = wchan_create(cv->cv_name);
	if (cv->cv_wchan == NULL) {
		kfree(cv->cv_name);
		kfree(cv);
		return NULL;
	}

	spinlock_init(&cv->cv_spinlock);
	// add stuff here as needed

	return cv;
}

void
cv_destroy(struct cv *cv)
{
	KASSERT(cv != NULL);

	// add stuff here as needed
	spinlock_cleanup(&cv->cv_spinlock);
	wchan_destroy(cv->cv_wchan);

	kfree(cv->cv_name);
	kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	// Write this
	KASSERT(cv != NULL);
	KASSERT(lock != NULL);
	KASSERT(lock_do_i_hold(lock) == true);
	
	spinlock_acquire(&cv->cv_spinlock);
	lock_release(lock);
	wchan_sleep(cv->cv_wchan, &cv->cv_spinlock);
	spinlock_release(&cv->cv_spinlock);
	lock_acquire(lock); // acquiring lock after releaseing cv_spinlock as cpu can hold only one spinlock at a time, Also this is correct as the thread should continue running
						// immediately after waking up
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	// Write this
	KASSERT(cv != NULL);
	KASSERT(lock != NULL);
	KASSERT(lock_do_i_hold(lock) == true);

    spinlock_acquire(&cv->cv_spinlock);
    wchan_wakeone(cv->cv_wchan, &cv->cv_spinlock);
    spinlock_release(&cv->cv_spinlock);
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	// Write this
	KASSERT(cv != NULL);
	KASSERT(lock != NULL);
	KASSERT(lock_do_i_hold(lock) == true);     

	spinlock_acquire(&cv->cv_spinlock);
	wchan_wakeall(cv->cv_wchan, &cv->cv_spinlock);
	spinlock_release(&cv->cv_spinlock);

}

//RW

struct rwlock * rwlock_create(const char *name)
{

	struct rwlock  *rw;

	rw = kmalloc(sizeof(*rw));
	if (rw == NULL) {
		return NULL;
	}

	rw->rwlock_name = kstrdup(name);
	if (rw->rwlock_name==NULL) {
		kfree(rw);
		return NULL;
	}

	rw->rwlock_read_wchan = wchan_create(rw->rwlock_name);
	if (rw->rwlock_read_wchan == NULL) {
		kfree(rw->rwlock_name);
		kfree(rw);
		return NULL;
	}

	rw->rwlock_write_wchan = wchan_create(rw->rwlock_name);
	if (rw->rwlock_write_wchan == NULL) {
		kfree(rw->rwlock_name);
        kfree(rw->rwlock_read_wchan);
        kfree(rw);
		return NULL;
	}

	/*rw->rwlock_cv_lock = cv_create(name);
    if (rw->rwlock_cv_lock == NULL) {
        kfree(rw->rwlock_cv_lock);
        kfree(rw->rwlock_read_wchan);
        kfree(rw->rwlock_write_wchan);
        kfree(rw);
        return NULL;
    }*/
    // add null check
	/*rw->rwlock_read_lock = lock_create(name);
	if (rw->rwlock_read_lock == NULL) {
        kfree(rw->rwlock_cv_lock);
        kfree(rw->rwlock_read_wchan);
        kfree(rw->rwlock_write_wchan);
        kfree(rw->rwlock_read_lock);
        kfree(rw);
        return NULL;
    }*/
	rw->rwlock_write_lock = lock_create(name);
	if (rw->rwlock_write_lock == NULL) {
        //kfree(rw->rwlock_cv_lock);
        kfree(rw->rwlock_read_wchan);
        kfree(rw->rwlock_write_wchan);
        //kfree(rw->rwlock_read_lock);
        //kfree(rw->rwlock_write_lock);
        kfree(rw);
    }
	spinlock_init(&rw->rwlock_spinlock);
	rw->rwlock_next_thread = NONE;
	rw->rwlock_readers_count = 0;
	rw->is_held_by_writer = false;

	//threadlist_init(&rw->readers_list);
	return rw;
}

bool rwlock_do_i_hold(struct rwlock* rwlock)
{
	KASSERT(rwlock != NULL);
	//kprintf("threadlist count: %d\n", rwlock->readers_list.tl_count);
    /*THREADLIST_FORALL(iterval, rwlock->readers_list) {
        kprintf("hold: %p \n", iterval);
        if(iterval == curthread)
            return true;
    }*/
	// this would not give results as expected because when reader acquires lock we are not setting rwlock_curthread variable.
	return (rwlock->rwlock_curthread == curthread);

}
void rwlock_destroy(struct rwlock *rwlock) {
	KASSERT(rwlock != NULL);
	//KASSERT(rwlock_do_i_hold(rwlock) == false);
    KASSERT(rwlock->rwlock_readers_count == 0);
    KASSERT(rwlock->is_held_by_writer == false);
	//kprintf("threadlist count: %d\n", rwlock->readers_list.tl_count);
	spinlock_cleanup(&rwlock->rwlock_spinlock);
	wchan_destroy(rwlock->rwlock_read_wchan);
	wchan_destroy(rwlock->rwlock_write_wchan);
	lock_destroy(rwlock->rwlock_write_lock);
	//cv_destroy(rwlock->rwlock_cv_lock);
	//threadlist_cleanup(&rwlock->readers_list);
	kfree(rwlock->rwlock_name);
	kfree(rwlock);
}

void rwlock_acquire_read(struct rwlock *rwlock) {
	KASSERT(rwlock != NULL);
	KASSERT(curthread->t_in_interrupt == false);
	//KASSERT(rwlock_do_i_hold(rwlock) == false);

    spinlock_acquire(&rwlock->rwlock_spinlock);
	// if there is a writer in the critical section or if the next thread to be executed is a writer and there is atleast one waiting in the queue.
	while(rwlock->is_held_by_writer == true || 
	        (rwlock->rwlock_next_thread == WRITER && 
	        !wchan_isempty(rwlock->rwlock_write_wchan, &rwlock->rwlock_spinlock
	            ))) {
		wchan_sleep(rwlock->rwlock_read_wchan, & rwlock->rwlock_spinlock);
        //kprintf("rwlock_acquire_read: Reader woke up!\n");
        //kprintf("rwlock_acquire_read: isheldwriter[%d] nextThread[%d]\n", rwlock->is_held_by_writer, rwlock->rwlock_next_thread);
    }

	KASSERT(rwlock->is_held_by_writer == false);
	rwlock->rwlock_next_thread = WRITER;
	rwlock->rwlock_readers_count++;
	//threadlist_addhead(&rwlock->readers_list, curthread);
	//kprintf("curthread: %p\n", curthread);
	spinlock_release(&rwlock->rwlock_spinlock);
}

void rwlock_release_read(struct rwlock *rwlock) {
	KASSERT(rwlock != NULL);
    //KASSERT(rwlock_do_i_hold(rwlock) == true);
//	If the current reader holds lock then it must exist in readers_list. It would be sufficient to check that 
//	Add the above condition in rwlock_do_i_hold
// How do we check if the thread that is calling this function has indeed called acquire. Create a readers list?
	spinlock_acquire(&rwlock->rwlock_spinlock);
	bool is_writer_waiting = !wchan_isempty(rwlock->rwlock_write_wchan, &rwlock->rwlock_spinlock);
	//bool are_readers_waiting = !wchan_isempty(rwlock->rwlock_read_wchan, &rwlock->rwlock_spinlock);
	if(rwlock->rwlock_next_thread == WRITER && is_writer_waiting && rwlock->rwlock_readers_count == 1) {
		//this is the last thread in the readers list, wake up the writer
			wchan_wakeone(rwlock->rwlock_write_wchan, &rwlock->rwlock_spinlock);
        //kprintf("rwlock_acquire_release: Reader woke up!");
    }
	// if reached here, then there is no writer in the wait chan or there are other readers in the critical section, safe to wake up all sleeping read threads
	//if(are_readers_waiting)
	//	wchan_wakeall(rwlock->rwlock_read_wchan, &rwlock->rwlock_spinlock);
	//kprintf("threadlist count: %d\n", rwlock->readers_list.tl_count);
	rwlock->rwlock_readers_count--;
	
	KASSERT(rwlock->rwlock_readers_count >= 0);
	spinlock_release(&rwlock->rwlock_spinlock);
}

void rwlock_acquire_write(struct rwlock *rwlock) {
	
    KASSERT(curthread->t_in_interrupt == false);
    KASSERT(rwlock != NULL);
	//KASSERT(rwlock_do_i_hold(rwlock) == false);
    
	spinlock_acquire(&rwlock->rwlock_spinlock);
	
	// if there is one writer or alteast one reader in the critical section then we could make this writer sleep
	while(rwlock->is_held_by_writer == true ||  rwlock->rwlock_readers_count != 0) {
		//kprintf("Writer Going to sleep\n");
		wchan_sleep(rwlock->rwlock_write_wchan, &rwlock->rwlock_spinlock);
	    //kprintf("Writer woke up!\n");
	    //kprintf("isheldbyWriter[%d] readersCount[%d]\n", rwlock->is_held_by_writer, rwlock->rwlock_readers_count);
	}

	KASSERT(rwlock->is_held_by_writer == false);
	//kprintf("Acquired write lock\n");
	rwlock->rwlock_curthread = curthread;
	rwlock->is_held_by_writer = true;
	rwlock->rwlock_next_thread = READER;
	spinlock_release(&rwlock->rwlock_spinlock);
}

void rwlock_release_write(struct rwlock *rwlock) {
	KASSERT(rwlock != NULL);
	KASSERT(rwlock_do_i_hold(rwlock));
    //KASSERT(rwlock->rwlock_next_thread == READER);
// writer is done, should i wake up all sleeping readers or a writer now? use a toggle mechanism as of now
	spinlock_acquire(&rwlock->rwlock_spinlock);
	rwlock->is_held_by_writer = false;
	if(!wchan_isempty(rwlock->rwlock_read_wchan, &rwlock->rwlock_spinlock)) {
		wchan_wakeall(rwlock->rwlock_read_wchan, &rwlock->rwlock_spinlock);
		//rwlock->rwlock_next_thread = WRITER;
	} else if(!wchan_isempty(rwlock->rwlock_write_wchan, &rwlock->rwlock_spinlock)) {
		wchan_wakeone(rwlock->rwlock_write_wchan, &rwlock->rwlock_spinlock);
		//rwlock->rwlock_next_thread = READER;
	} else {
        rwlock->rwlock_next_thread = NONE;
    }
	spinlock_release(&rwlock->rwlock_spinlock);
}
