#ifndef LINKEDLIST
#define LINKEDLIST
struct list_node
{
	void* node;
	struct list_node* next;

};


int add_node(struct list_node** head, void* node_to_be_added);
void delete_node(struct list_node** head, void* node_to_be_deleted);
void delete_list(struct list_node** head);

#endif
