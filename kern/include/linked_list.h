#ifndef LINKEDLIST
#define LINKEDLIST
struct list_node
{
	void* node;
	struct list_node* next;

};

struct as_region* as_region;
struct page_table_entry* pt;

int region_list_add_node(struct as_region** head, struct as_region* node_to_be_added);
void region_list_delete_node(struct as_region** head, struct as_region* node_to_be_deleted);
void region_list_delete(struct as_region** head);

int page_list_add_node(struct page_table_entry** head, struct page_table_entry* node_to_be_added);
void page_list_delete_node(struct page_table_entry** head, struct page_table_entry* node_to_be_deleted);
void page_list_delete(struct page_table_entry** head);


int add_node(struct list_node** head, void* node_to_be_added);
void delete_node(struct list_node** head, void* node_to_be_deleted);
void delete_list(struct list_node** head);

#endif
