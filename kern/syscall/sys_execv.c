#include <kern/sys_execv.h>
#include <syscall.h>
#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <current.h>
#include <proc.h>
#include <pid.h>
#include <kern/wait.h>
#include <vfs.h>
#include <copyinout.h>
#include <limits.h>
#include <vm.h>
#include <addrspace.h>
//extern struct semaphore* esem;

extern bool swapping_started;	


int sys_execv(userptr_t progname, userptr_t *arguments) {
    struct proc *proc = curproc;
    struct addrspace *as;
    struct vnode *v;
    vaddr_t entrypoint, stackptr;
    int result;

    //kprintf("****EXECV]***** process-%d trying to exec", proc->pid);

    //lock_acquire(execlock);
    //kprintf("****EXECV]***** process-%d acquired exec lock", proc->pid);
	if(progname == NULL || progname == (void *)0x80000000 || progname == (void *)0x40000000) {
		return EFAULT;
	}

    if(arguments == NULL || arguments == (void *)0x80000000 || arguments == (void *)0x40000000) {
        return EFAULT;
    }
    /* This process should have an address space copied during fork */
    KASSERT(proc != NULL);
    char *_progname;
    size_t size;
    int i=0, count=0;
    _progname = (char *) kmalloc(sizeof(char)*PATH_MAX);
    result = copyinstr(progname, _progname, PATH_MAX, &size);
    if(result) {
        kfree(_progname);
        return EFAULT;
    }
    if(strlen(_progname) == 0) {
        kfree(_progname);
        return EINVAL;
    }
	if(swapping_started == true) {
    }
    kfree(_progname);

    char *args = (char *) kmalloc(sizeof(char)*ARG_MAX);
    result = copyinstr((const_userptr_t)arguments, args, ARG_MAX, &size);
    if(result) {
        kfree(args);
        return EFAULT;
    }
    /* Copy the user arguments on to the kernel */
   
    int offset = 0;
    while((char *) arguments[count] != NULL) {
        result = copyinstr((const_userptr_t) arguments[count], args+offset, ARG_MAX, &size);
        if(result) {
            kfree(args);
            return EFAULT;
        }
        offset += size;
        count++;
    }

    /* Open the file */
    result = vfs_open((char *)progname, O_RDONLY, 0, &v);
    if(result) {
        kfree(args);
        return result;
    }

    /* Destroy the current address space and Create a new address space */
    as_destroy(proc->p_addrspace);
    proc->p_addrspace = NULL;
    
    KASSERT(proc_getas() == NULL);

    as = as_create();
    if(as == NULL) {
        kfree(args);
        vfs_close(v);
        return ENOMEM;
    }
    /* Switch to it and activate it */
    proc_setas(as);
    as_activate();

    /* Load the executable. */

  //  kprintf("free pages available before load_elf : %d \n", coremap_free_bytes()/4096);
    result = load_elf(v, &entrypoint);
    if(result) {
        kfree(args);
        vfs_close(v);
        return result;
    }

    /* Done with the file now */
    vfs_close(v);

    /* Define the user stack in the address space */
    result = as_define_stack(as, &stackptr);
    if(result) {
        kfree(args);
        return result;
    }

    i = 0;
    int prevlen = 0, cur = 0;
    char **stkargs=(char**) kmalloc(sizeof(char*)*(count+1));
    while(i < offset) {
        int len=0, olen;
        for(len=0; args[i] != '\0'; len++, i++);
        olen = len;
        len = len + (4 - len%4);

        char *arg = kmalloc(len);
        //arg = kstrdup(args+prevlen);

        //kprintf("%s\n", arg);
        int j = prevlen;
        for(int k=0; k<len; k++) {
            if(k >= olen)
                arg[k] = '\0';
            else
                arg[k] = args[j++];
        }

        //kprintf("%s\n", arg);
        stackptr -= len;
        result = copyout((const void *)arg, (userptr_t)stackptr, (size_t)len);
        if(result) {
            kfree(args);
            kfree(stkargs);
            return result;
        }
        kfree(arg);
        stkargs[cur++] = (char *)stackptr;
        prevlen += olen + 1;
        i++;
    }
    stkargs[cur] = NULL;
    kfree(args);

    for(i=(cur) ; i>=0; i--) {
        stackptr = stackptr - sizeof(char *);
        //kprintf("copying arg %d at [%p]\n", i, *(stkargs+i));
        result = copyout((const void *)(stkargs+i), (userptr_t) stackptr, (sizeof(char *)));
        if(result) {
            kfree(stkargs);
            return result;
        }
        prevlen += 4;
    }
    kfree(stkargs);
    
    //unsigned int free = coremap_free_bytes();
  //  unsigned int free_pages = free/4096;

  //  kprintf("free pages available : %d \n", free_pages);

    //lock_release(execlock);
    //kprintf("****EXECV]***** process-%d released exec lock", proc->pid);
    enter_new_process(count, (userptr_t) stackptr, NULL, stackptr, entrypoint);

    panic("enter_new_process returned\n");
    return EINVAL;
}
