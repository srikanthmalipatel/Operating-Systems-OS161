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

int sys_execv(userptr_t progname, userptr_t *arguments) {
    struct proc *proc = curproc;
    struct addrspace *as;
    struct vnode *v;
    vaddr_t entrypoint, stackptr;
    int result;

    /* This process should have an address space copied during fork */
    KASSERT(proc != NULL);
   
    char *_progname;
    size_t size;
    int i=0;
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
    char **args = (char **) kmalloc(sizeof(char *));
    result = copyin((const_userptr_t)arguments, args, sizeof(char **));
    if(result) {
        kfree(args);
        kfree(_progname);
        return EFAULT;
    }
    while((char *) arguments[i] != NULL) {
        if(i == 0) {
            args[i] = (char *) kmalloc(sizeof(char)*128);
            result = copyinstr((const_userptr_t) arguments[i], args[i], (size_t) 128, &size);
        } else {
            args[i] = (char *) kmalloc(sizeof(char)*70000);
            result = copyinstr((const_userptr_t) arguments[i], args[i], (size_t) 70000, &size);
        }
        if(result) {
            kfree(args);
            kfree(_progname);
            return EFAULT;
        }
        i++;
    }
    args[i] = NULL;
    
    
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

    /* Copy the user arguments into the address space. First copy the arguments from user space into kernel space */
    i = 0;
    while(args[i] != NULL) {
        char *buf;
        int j = 0, len;
        len = strlen(args[i]) + 1;
        
        int prevlen = len;
        if(len%4 != 0)
            len = len + (4 - len%4);

        buf = (char *) kmalloc(sizeof(len));
        buf = kstrdup(args[i]);
        for(j=0; j<len; j++) {
            if(j >= prevlen)
                buf[j] = '\0';
            else
                buf[j] = args[i][j];
        }

        stackptr -= len;
        result = copyout((const void*) buf, (userptr_t) stackptr, (size_t)len);
        if(result) {
            kfree(_progname);
            kfree(args);
            kfree(buf);
            return result;
        }
        kfree(buf);
        kfree(args[i]);
        args[i] = (char *) stackptr;
        i++;
    }

    if(args[i] == NULL) {
        stackptr -= 4*sizeof(char);
    }
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
    kfree(args);

    enter_new_process(i, (userptr_t) stackptr, NULL, stackptr, entrypoint);

    panic("enter_new_process returned\n");
    return EINVAL;
}
