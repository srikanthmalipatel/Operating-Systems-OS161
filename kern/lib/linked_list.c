#include <linked_list.h>
#include <types.h>
#include <lib.h>
#include <addrspace.h>


int region_list_add_node(struct as_region **head, struct as_region* node_to_add)
{
	if(head == NULL)
		return -1;
	
	if(*head == NULL)
	{
		// first node.
		*head = node_to_add;
//		(*head)->node = node_to_add;
		(*head)->next = NULL;
	
	}
	else
	{
		struct as_region* temp = *head;
		while(temp->next != NULL)
			temp = temp->next;


		temp->next = node_to_add;
		node_to_add->next = NULL;
	
	}
	
	return 0;
}

void region_list_delete_node(struct as_region** head, struct as_region* node_to_delete)
{
	if(head == NULL || *head == NULL)
		return;
	
	if((*head) == node_to_delete)
	{
		kfree(*head);
		*head = NULL;
	
	}
	else
	{
		struct as_region* temp = *head;
		while(temp->next != NULL && temp->next != node_to_delete)
			temp = temp->next;

		if(temp->next == NULL)
			return;

		struct as_region* next_node = temp->next->next;
		kfree(temp->next);
		temp->next = next_node;

		return;
	
	}

}

void region_list_delete(struct as_region** head)
{
	if(head == NULL)
		return;
	
	struct as_region* cur = *head;
	struct as_region* next = NULL;
	while(cur != NULL)
	{
		next = cur->next;
//		kfree(cur->node);
		kfree(cur);
		cur = next;
	
	}
	*head = NULL;

	return;

}

int page_list_add_node(struct page_table_entry **head, struct page_table_entry* node_to_add)
{
	if(head == NULL)
		return -1;
	
	if(*head == NULL)
	{
		// first node.

		*head = node_to_add;
	//	(*head)->node = node_to_add;
		(*head)->next = NULL;
	
	}
	else
	{
		struct page_table_entry* temp = *head;
		while(temp->next != NULL)
			temp = temp->next;

		temp->next = node_to_add;
		node_to_add->next = NULL;
	
	}
	
	return 0;
}

void page_list_delete_node(struct page_table_entry** head, struct page_table_entry* node_to_delete)
{
	if(head == NULL || *head == NULL)
		return;
	
	if((*head) == node_to_delete)
	{
	//	kfree(node_to_delete);
		kfree(*head);
		*head = NULL;
	
	}
	else
	{
		struct page_table_entry* temp = *head;
		while(temp->next != NULL && temp->next != node_to_delete)
			temp = temp->next;

		if(temp->next == NULL)
			return;

		struct page_table_entry* next_node = temp->next->next;
		kfree(temp->next);
		temp->next = next_node;

		return;
	
	}

}

void page_list_delete(struct page_table_entry** head)
{
	if(head == NULL)
		return;
	
	struct page_table_entry* cur = *head;
	struct page_table_entry* next = NULL;
	while(cur != NULL)
	{
		next = cur->next;
		//kfree(cur->node);
		kfree(cur);
		cur = next;
	
	}
	*head = NULL;

	return;

}

int add_node(struct list_node **head, void* node_to_add)
{
	if(head == NULL)
		return -1;
	
	if(*head == NULL)
	{
		// first node.
		*head = (struct list_node*)kmalloc(sizeof(struct list_node));
		if(*head == NULL)
			return -1;

		(*head)->node = node_to_add;
		(*head)->next = NULL;
	
	}
	else
	{
		struct list_node* temp = *head;
		while(temp->next != NULL)
			temp = temp->next;

		struct list_node* new_node = (struct list_node*)kmalloc(sizeof(struct list_node));
		if(new_node == NULL)
			return -1;

		temp->next = new_node;
		new_node->node = node_to_add;
		new_node->next = NULL;
	
	}
	
	return 0;
}

void delete_node(struct list_node** head, void* node_to_delete)
{
	if(head == NULL || *head == NULL)
		return;
	
	if((*head)->node == node_to_delete)
	{
		kfree(node_to_delete);
		kfree(*head);
		*head = NULL;
	
	}
	else
	{
		struct list_node* temp = *head;
		while(temp->next != NULL && temp->next->node != node_to_delete)
			temp = temp->next;

		if(temp->next == NULL)
			return;

		struct list_node* next_node = temp->next->next;
		kfree(temp->next->node);
		kfree(temp->next);
		temp->next = next_node;

		return;
	
	}

}

void delete_list(struct list_node** head)
{
	if(head == NULL)
		return;
	
	struct list_node* cur = *head;
	struct list_node* next = NULL;
	while(cur != NULL)
	{
		next = cur->next;
		kfree(cur->node);
		kfree(cur);
		cur = next;
	
	}
	*head = NULL;

	return;

}
