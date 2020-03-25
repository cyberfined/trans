#pragma once

typedef struct node_s {
    struct node_s *next;
} node_t;

typedef node_t* (*node_copy_func)(node_t*);
typedef void (*node_free_func)(node_t*);
typedef struct {
    node_t         *first;
    node_t         *last;
    node_copy_func copy_func;
    node_free_func free_func;
} list_t;

list_t* list_create(node_copy_func copy_func, node_free_func free_func);
void _list_prepend(list_t *l, node_t *n);
void _list_append(list_t *l, node_t *n);
list_t* list_union(list_t *l1, list_t *l2);
list_t* list_copy(list_t *l);
void list_free(list_t *l);

#define list_prepend(l, n) _list_prepend(l, (node_t*)(n))

#define list_append(l, n) _list_append(l, (node_t*)(n))

#define list_for_each(var, type, ptr) \
    for(type *var = (type*)ptr->first; var != NULL; var = (type*)var->next)
