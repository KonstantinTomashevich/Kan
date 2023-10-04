#include <kan/container/list.h>

void kan_bd_list_init (struct kan_bd_list_t *list)
{
    list->size = 0u;
    list->first = NULL;
    list->last = NULL;
}

void kan_bd_list_add (struct kan_bd_list_t *list,
                      struct kan_bd_list_node_t *before,
                      struct kan_bd_list_node_t *node)
{
    ++list->size;
    node->next = before;

    if (before)
    {
        node->previous = before->previous;
        if (before->previous)
        {
            before->previous->next = node;
        }
        else
        {
            list->first = node;
        }
    }
    else
    {
        node->previous = list->last;
        if (list->last)
        {
            list->last->next = node;
            list->last = node;
        }
        else
        {
            list->last = node;
            list->first = node;
        }
    }
}

void kan_bd_list_remove (struct kan_bd_list_t *list, struct kan_bd_list_node_t *node)
{
    --list->size;
    if (node->next)
    {
        node->next->previous = node->previous;
    }
    else
    {
        list->last = node->previous;
    }

    if (node->previous)
    {
        node->previous->next = node->next;
    }
    else
    {
        list->first = node->next;
    }
}
