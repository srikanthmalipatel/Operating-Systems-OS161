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
	KASSERT(fh != NULL);
	struct lock* lk = lock_create("fh lock");
	KASSERT(lk != NULL);
	memset(fh->file_name, 0, sizeof(fh->file_name));
	fh->offset = 0;
	fh->openflags = -1;
	fh->file = NULL;
	fh->ref_count = -1;
	fh->fh_lock = lk;

	return fh;

}

// called when thread is abruptly destroyed with files still open,call close on  all the files this thread is currently holding.
void file_table_cleanup(struct file_handle** ft)
{
	(void)ft;





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
	kfree(fh->file_name);
	kfree(fh);

}



