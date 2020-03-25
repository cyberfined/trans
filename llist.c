#include "llist.h"

#include <stdio.h>
#include <stdlib.h>

list_t* list_create(node_copy_func copy_func, node_free_func free_func) {
    list_t *l = malloc(sizeof(list_t));
    if(!l) {
        perror("malloc");
        return NULL;
    }

    l->first = NULL;
    l->last = NULL;
    l->copy_func = copy_func;
    l->free_func = free_func;
    return l;
}

void _list_prepend(list_t *l, node_t *n) {
    n->next = l->first;
    l->first = n;
    if(!l->last)
        l->last = n;
}

void _list_append(list_t *l, node_t *n) {
    if(!l->first) {
        l->first = n;
        l->last = n;
    } else {
        l->last->next = n;
        l->last = n;
    }
    n->next = NULL;
}

list_t* list_union(list_t *l1, list_t *l2) {
    if(!l1->last)
        l1->first = l2->first;
    else
        l1->last->next = l2->first;
    l1->last = l2->last;
    free(l2);
    return l1;
}

list_t* list_copy(list_t *l) {
    if(!l->copy_func)
        return NULL;

    list_t *cpy = list_create(l->copy_func, l->free_func);
    if(!cpy)
        goto exit;

    list_for_each(i, node_t, l) {
        node_t *n = l->copy_func(i);
        if(!n)
            goto exit;
        list_append(cpy, n);
    }

    return cpy;
exit:
    if(cpy) list_free(cpy);
    return NULL;
}

void list_free(list_t *l) {
    node_t *i = l->first, *tmp;
    while(i != NULL) {
        tmp = i->next;
        if(l->free_func)
            l->free_func(i);
        free(i);
        i = tmp;
    }
    free(l);
}
