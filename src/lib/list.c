#include <stdlib.h>
#include "list.h"

static listnode_t *node_New();
static void node_Free(listnode_t *n);
static void remove_node(list_t *l, listnode_t *n);

list_t *list_New()
{
    list_t *l = malloc(sizeof(list_t));
    if(l == NULL)
        return NULL;

    l->count = 0;
    l->first = NULL;
    l->last = NULL;
    return l;
}

void list_Free(list_t *l)
{
    if(l == NULL)
        return;

    listnode_t *n = l->first;
    while(n != NULL)
    {
        listnode_t *next = n->next;
        node_Free(n);
        n = next;
    }

    free(l);
}

void list_Add(list_t *l, void *ptr)
{
    listnode_t *n = node_New();
    if(n == NULL)
        return;

    n->ptr = ptr;
    n->next = l->first;
    n->prev = NULL;

    l->count++;

    if(l->first != NULL)
        l->first->prev = n;
    l->first = n;
    if(l->last == NULL)
        l->last = l->first;
}

void list_Append(list_t *l, void *ptr)
{
    listnode_t *n = node_New();
    if(n == NULL)
        return;

    n->ptr = ptr;
    n->next = NULL;
    n->prev = l->last;

    l->count++;

    if(l->last != NULL)
        l->last->next = n;
    l->last = n;
    if(l->first == NULL)
        l->first = l->last;
}

static void remove_node(list_t *l, listnode_t *n)
{
    if(l->first == n)
        l->first = n->next;
    if(l->last == n)
        l->last = n->prev;
    if(n->next != NULL)
        n->next->prev = n->prev;
    if(n->prev != NULL)
        n->prev->next = n->next;

    node_Free(n);
}

static listnode_t *node_New()
{
    listnode_t *n = malloc(sizeof(listnode_t));
    if(n == NULL)
        return NULL;

    n->next = NULL;
    n->prev = NULL;
    n->ptr = NULL;

    return n;
}

static void node_Free(listnode_t *n)
{
    if(n != NULL)
        free(n);
}
