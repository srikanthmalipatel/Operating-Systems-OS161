/*
 * All the contents of this file are overwritten during automated
 * testing. Please consider this before changing anything in this file.
 */

#include <types.h>
#include <lib.h>
#include <clock.h>
#include <thread.h>
#include <synch.h>
#include <test.h>
#include <kern/test161.h>
#include <spinlock.h>

#define NUMREADERS1     120 
#define NUMWRITERS1     50
#define CREATELOOPS     8

static volatile unsigned long testval1;
static volatile unsigned long testval2;
static volatile unsigned long testval3;

static struct semaphore *donesem = NULL;
static struct rwlock    *testrwlock = NULL;

struct spinlock status_lock;
static bool test_status = FAIL;
/*
 * Use these stubs to test your reader-writer locks.
 */

/*static
bool
failif(bool condition) {
    if(condition) {
        spinlock_acquire(&status_lock);
        test_status = FAIL;
        spinlock_release(&status_lock);
    }
    return condition;
}*/

static
void readerthread(void *junk, unsigned long num) {
    (void)junk;
    if(num < 80) {
        //kprintf_n("Thread %lu trying to acquire read lock\n", num);
        random_yielder(4);
        rwlock_acquire_read(testrwlock);
        random_yielder(4);
        kprintf_n("Thread %lu: Reader val:%lu\n", num, testval1);
        for(int i=0; i<40000; i++);
        random_yielder(4);
        rwlock_release_read(testrwlock);
        random_yielder(4);
        //kprintf_n("Thread %lu released read lock\n", num);
        V(donesem);
    }
    else {
        //kprintf_n("Thread %lu trying to acquire write lock\n", num);
        random_yielder(4);
        rwlock_acquire_write(testrwlock);
        random_yielder(4);
        testval1++;
        kprintf_n("Thread %lu: Writer val:%lu\n", num, testval1);
        for(int i=0; i<40000; i++);
        random_yielder(4);
        rwlock_release_write(testrwlock);
        random_yielder(4);
        //kprintf_n("Thread %lu released write lock\n", num);
        V(donesem);
    }
    return;
}
/*
static
void writerthread(void *junk, unsigned long num) {
    (void)junk;
    rwlock_acquire_write(testrwlock);
    random_yielder(4);
    testval1++;
    kprintf_n("Thread %lu: Writer val:%lu\n", num, testval1);
    random_yielder(4);
    rwlock_release_write(testrwlock);
    V(donesem);
    return;
}*/
int 
rwtest(int nargs, char **args) 
{
	(void)nargs;
	(void)args;

    int i, result;

	kprintf_n("Starting rwt1...\n");
    for (i=0; i<CREATELOOPS; i++) {
        kprintf_t(".");
        testrwlock = rwlock_create("testreaderwriterlock");
        if (testrwlock == NULL) {
            panic("rwt1: rwlock_create failed\n");
        }
        donesem = sem_create("donesem", 0);
        if (donesem == NULL) {
            panic("rtw1: sem_create failed\n");
        }
        if(i != CREATELOOPS -1) {
            rwlock_destroy(testrwlock);
            sem_destroy(donesem);
        }
    }
    spinlock_init(&status_lock);
    test_status = SUCCESS;

    testval1 = 0;
    // create threads code
    for (i=0; i<NUMREADERS1; i++) {
        kprintf_t(".");
        result = thread_fork("rwlockstest", NULL, readerthread, NULL, i);
        if(result) {
            panic("rwt1: thread_fork failed: %s\n", strerror(result));
        }
    }

    /*for (i=0; i<NUMWRITERS1; i++) {
        kprintf_t(".");
        result = thread_fork("rwlockstest", NULL, writerthread, NULL, i);
        if(result) {
            panic("rwt1: thread_fork failed: %s\n", strerror(result));
        }
    }*/
    kprintf("%d\n", i);
    int numthreads = NUMREADERS1;
    for (i=0; i<numthreads; i++) {
        kprintf_t(".");
        P(donesem);
    }
    
    rwlock_destroy(testrwlock);
    sem_destroy(donesem);
    testrwlock = NULL;
    donesem = NULL;
    
    kprintf_t("\n");
	success(TEST161_FAIL, SECRET, "rwt1");

	return 0;
}

int rwtest2(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("rwt2 unimplemented\n");
	success(TEST161_FAIL, SECRET, "rwt2");

	return 0;
}

int rwtest3(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("rwt3 unimplemented\n");
	success(TEST161_FAIL, SECRET, "rwt3");

	return 0;
}

int rwtest4(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("rwt4 unimplemented\n");
	success(TEST161_FAIL, SECRET, "rwt4");

	return 0;
}

int rwtest5(int nargs, char **args) {
	(void)nargs;
	(void)args;

	kprintf_n("rwt5 unimplemented\n");
	success(TEST161_FAIL, SECRET, "rwt5");

	return 0;
}
