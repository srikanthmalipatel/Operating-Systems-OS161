
#ifndef _FILESYSTEM_H_
#define _FILESYSTEM_H_

#include<vnode.h>
#include<synch.h>
#include<limits.h>
#include<types.h>
struct file_handle
{
	char file_name[NAME_MAX + 1]; 	// obvious
	off_t offset;  					// current offset on the file
	int openflags;     				// read or write?
	struct vnode* file; 			// pointer to file on disc
	struct lock* fh_lock;  			// multiple processes can share the same file handle, need lock to synchronize
	int ref_count;  				// count of the number of times this is being used, delete when count becomes 0

};


void file_table_cleanup(struct file_handle **ft);
void file_table_copy(struct file_handle **ft1, struct file_handle **ft2);
struct file_handle* file_handle_create(void);
struct file_handle* get_file_handle(struct file_handle**, int);
void file_table_init(struct file_handle** );
void file_table_destroy(struct file_handle** );
int get_free_file_descriptor(struct file_handle** );
void file_handle_destroy (struct file_handle* fh);
bool is_valid_flag(int);
bool can_read(int);
bool can_write(int);
bool is_append(int);

#endif // FILESYSTEM_H
