#ifndef LIST_H
#define LIST_H
typedef struct _listnode
{
    struct _listnode *next;
    struct _listnode *prev;

    void *ptr;
} listnode_t;

typedef struct
{
    int count;
    listnode_t *first;
    listnode_t *last;
} list_t;

typedef int (comparator_t)(void *a, void *b);

// Constructor
list_t *list_New();

// Destructor
void list_Free(list_t *l);

// Add a new element (head)
void list_Add(list_t *l, void *ptr);

// Add a new element (tail)
void list_Append(list_t *l, void *ptr);
#endif
