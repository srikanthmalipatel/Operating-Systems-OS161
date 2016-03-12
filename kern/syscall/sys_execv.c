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

struct semaphore *esem;

int sys_execv(userptr_t progname, userptr_t *arguments) {
    struct proc *proc = curproc;
    struct addrspace *as;
    struct vnode *v;
    vaddr_t entrypoint, stackptr;
    int result;

    if(arguments == NULL) {
        return EFAULT;
    }
    /* This process should have an address space copied during fork */
    KASSERT(proc != NULL);
    P(esem);
    char *_progname;
    size_t size;
    int i=0, count=0;
    _progname = (char *) kmalloc(sizeof(char)*1024);
    result = copyinstr(progname, _progname, 1024, &size);
    if(result) {
        kfree(_progname);
        return EFAULT;
    }
    if(strlen(_progname) == 0) {
        kfree(_progname);
        return EINVAL;
    }
    
    char *args = (char *) kmalloc(sizeof(char)*4096);
    result = copyinstr((const_userptr_t)arguments, args, ARG_MAX, &size);
    if(result) {
        kfree(args);
        kfree(_progname);
        return EFAULT;
    }
    /* Copy the user arguments on to the kernel */

    int offset = 0;
    while((char *) arguments[count] != NULL) {
        result = copyinstr((const_userptr_t) arguments[count], args+offset, ARG_MAX, &size);
        if(result) {
            kfree(args);
            kfree(_progname);
            return EFAULT;
        }
        offset += size;
        count++;
    }


   /* while((char *) arguments[i] != NULL) {
        char *buf = (char *) kmalloc(sizeof(char)*70000);
        result = copyinstr((const_userptr_t) arguments[i], buf, (size_t) 70000, &size);
        if(result) {
            kfree(args);
            kfree(_progname);
            return EFAULT;
    }
        // get the length of the string and then increase the length if it is not four bytes aligned
      int len = strlen(buf); 
      int newlen = len + (4 - (strlen(buf) & 3));

        // copy the string into the arguments array 
        // TODO: we can copy the string from buf instead of doing a copyinstr. check if memory permits for this operation

        int j;
        args[i] = (char *) kmalloc(sizeof(char)*(newlen+2));

        strcpy(args[i], buf);
        // Add padding such that it is 4 bytes aligned
        // TODO: check if we are adding padding at right position
        for(j=len; j<=newlen; j++) {
            args[i][j] = '\0';
        }
        kfree(buf);
        i++;
    }
    args[i] = NULL;
    */
    
    /* Open the file */
    result = vfs_open((char *)progname, O_RDONLY, 0, &v);
    if(result) {
        return result;
    }

    /* Destroy the current address space and Create a new address space */
    as_destroy(proc->p_addrspace);
    proc->p_addrspace = NULL;
    
    KASSERT(proc_getas() == NULL);

    as = as_create();
    if(as == NULL) {
        kfree(args);
        kfree(_progname);
        vfs_close(v);
        return ENOMEM;
    }
    /* Switch to it and activate it */
    proc_setas(as);
    as_activate();

    /* Load the executable. */
    result = load_elf(v, &entrypoint);
    if(result) {
        vfs_close(v);
        return result;
    }

    /* Done with the file now */
    vfs_close(v);

    /* Define the user stack in the address space */
    result = as_define_stack(as, &stackptr);
    if(result) {
        return result;
    }
    // Copy the user arguments into the address space. First copy the arguments from user space into kernel space 
    /*for(i=0 ; i<offset; i++) {
        if(args[i] == '\0')
            kprintf("0");
        kprintf("%c", args[i]);
    }
    kprintf("\n");*/
    i = 0;
    //kprintf("%d\n", offset);
    int prevlen = 0, cur = 0;
    char **stkargs=(char**) kmalloc(sizeof(char*)*(count+1));
    while(i < offset) {
        int len=0, olen;
        for(len=0; args[i] != '\0'; len++, i++);
        olen = len;
        len = len + (4 - len%4);

        char *arg = kmalloc(sizeof(len));
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
            return result;
        }
        kfree(arg);
        stkargs[cur++] = (char *)stackptr;
        prevlen += olen + 1;
        i++;
    }
    stkargs[cur] = NULL;

    for(i=(cur) ; i>=0; i--) {
        stackptr = stackptr - sizeof(char *);
        //kprintf("copying arg %d at [%p]\n", i, *(stkargs+i));
        result = copyout((const void *)(stkargs+i), (userptr_t) stackptr, (sizeof(char *)));
        if(result)
            return result;
        prevlen += 4;
    }
    /*vaddr_t tmp = stackptr+prevlen;
    result = copyin((const_userptr_t)stackptr, args, ARG_MAX);
    for(i=0; i<prevlen; i++) {
        if(args[i] == '\0')
            kprintf("[%x] 0\n", tmp--);
        kprintf("[%x] %c\n", tmp--, args[i]);
    }
    kprintf("\n");*/


    /*i = 0;
    while(args[i] != NULL) {
        stackptr -= strlen(args[i]);
        result = copyout((const void*) args[i], (userptr_t) stackptr, (size_t)strlen(args[i]));
        if(result) {
            kfree(_progname);
            kfree(args);
            return result;
        }
        kfree(args[i]);
        args[i] = (char *) stackptr;
        i++;
    }
    stackptr -= 4*sizeof(char);
    
    int k;
    for(k = i-1; k>=0; k--) {
        stackptr = stackptr - sizeof(char *);
        result = copyout((const void*) (args + k), (userptr_t) stackptr, sizeof(char *));
        if(result) {
            kfree(_progname);
            kfree(args);
            return result;
        }
    }
    kfree(_progname);
    kfree(args);*/
    V(esem);

    enter_new_process(count, (userptr_t) stackptr, NULL, stackptr, entrypoint);

    panic("enter_new_process returned\n");
    return EINVAL;
}
