#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <array.h>
#include <cpu.h>
#include <spl.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <threadlist.h>
#include <threadprivate.h>
#include <proc.h>
#include <current.h>
#include <synch.h>
#include <kern/fcntl.h>
#include <file_close.h>
void file_table_init(struct file_handle** ft)
{
	for(int i = 0; i < OPEN_MAX; i++)
	{
		ft[i] = NULL;
	
	}


}

void file_table_destroy(struct file_handle** ft)
{
	(void)ft;


}

int get_free_file_descriptor(struct file_handle** ft)
{
	
	// we start with 3 because 0,1 and 2 are for the console.
	for(int i = 3; i < OPEN_MAX; i++)
	{
		if(ft[i] == NULL)
		return i;
	}

	return -1;


}

struct file_handle* file_handle_create()
{
	struct file_handle* fh = kmalloc(sizeof(struct file_handle));
	if(fh == NULL)
		return NULL ;
	struct lock* lk = lock_create("fh lock");
	if(lk == NULL)
		return NULL;
	memset(fh->file_name, 0, sizeof(fh->file_name));
	fh->offset = 0;
	fh->openflags = -1;
	fh->file = NULL;
	fh->ref_count = 0;
	fh->fh_lock = lk;

	return fh;

}

// called when thread is abruptly destroyed with files still open,call close on  all the files this thread is currently holding.
void file_table_cleanup()
{
	struct file_handle **ft = curproc->t_file_table;

	if(ft == NULL)
	        return;
    
    for(int i = 0 ; i < OPEN_MAX; i++)
    {
        if(ft[i] != NULL)
        {
            if(ft[i]->ref_count == 1)
                sys_close(i);
            else
                ft[i]->ref_count --;

            ft[i] = NULL;
        
        } 
    }
    kfree(*ft);
    ft = NULL;
}

void file_table_copy(struct file_handle **ft1, struct file_handle **ft2) {
    if(ft1 == NULL || ft2 == NULL)
        return;
    for(int i=0; i<OPEN_MAX; i++) {
        if(ft1[i] != NULL) {
            lock_acquire(ft1[i]->fh_lock);
            ft2[i] = ft1[i];
            ft1[i]->ref_count++;
            lock_release(ft1[i]->fh_lock);
        }
    }
    return;
}

struct file_handle* get_file_handle(struct file_handle** ft, int fd)
{
	if(fd < 0 || fd >= OPEN_MAX)
		return NULL;
	
	else
		return ft[fd];

}



void file_handle_destroy (struct file_handle* fh)
{
	lock_destroy(fh->fh_lock);
	fh->fh_lock = NULL;
//	kfree(fh->file_name);
	fh->file = NULL;
	kfree(fh);
	fh = NULL;

}

bool is_valid_flag(int flag)
{

	//only one of the O_RDONLY, O_WRONLY and O_RDWR should be given.
	// Also the value should be positive and should not exceed

	if(flag < 0 || flag > 127) // all flags are set, should not be greater than this.
		return false;
	
	
	int mode = flag & O_ACCMODE;
	if(mode != O_RDONLY && mode != O_WRONLY && mode != O_RDWR)
		return false;
	
	return true;

}

bool can_read(int flag)
{
	if(is_valid_flag(flag))
	{
		int mode = flag & O_ACCMODE;
		if(mode == O_RDONLY || mode == O_RDWR)
			return true;
	
	}

	return false;

}


bool can_write(int flag)
{
	if(is_valid_flag(flag))
	{
		int mode = flag & O_ACCMODE;
		if(mode == O_WRONLY || mode == O_RDWR)
			return true;
	
	
	}

	return false;

}

bool is_append(int flag)
{

	if(is_valid_flag (flag))
	{
		int mode = flag >> 5;
		if(mode & 1)
			return true;
	
	}
	

	return false;



}






