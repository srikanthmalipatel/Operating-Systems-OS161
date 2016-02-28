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

	return NULL;


}

// called when thread is abruptly destroyed with files still open,call close on  all the files this thread is currently holding.
void file_table_cleanup(struct file_handle** ft)
{
	(void)ft;





}



/*void file_handle_destroy (struct file_handle* fh)
{





}*/



