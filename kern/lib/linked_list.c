#include <linked_list.h>
#include <types.h>
#include <lib.h>

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
