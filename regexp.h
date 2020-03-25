#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "llist.h"
#include "htable.h"

typedef bool (*chr_pred)(char);

typedef struct syn_tree {
    enum { SYM, OR, AND, STAR } tag;
    union {
        struct {
            char chr;
            chr_pred pred;
        } sym;
        struct {
            struct syn_tree *s1;
            struct syn_tree *s2;
        } or;
        struct {
            struct syn_tree *s1;
            struct syn_tree *s2;
        } and;
        struct {
            struct syn_tree *s;
        } star;
    };
} syn_tree_t;

void print_syn_tree(syn_tree_t *s);
void syn_tree_free(syn_tree_t *s);
syn_tree_t* parse_regexp(const char *regexp, size_t len, size_t *off, bool *error);
syn_tree_t* regexp_ext(syn_tree_t *s1);

typedef struct sym_node_s {
    struct sym_node_s *next;
    syn_tree_t        *t;
    size_t            ind;
} sym_node_t;

typedef struct {
    list_t   *firstpos;
    list_t   *lastpos;
    list_t   **followpos;
    htable_t *regexp_ptrs;
    bool     nullable;
    size_t   num_syms, max_syms;
} regexp_stat;

regexp_stat* get_regexp_stat(syn_tree_t *ext, htable_t *regexp_ptrs);
void free_regexp_stat(regexp_stat *st);
syn_tree_t* regexp_ext(syn_tree_t *s1);

typedef struct {
    uint32_t   hash;
    syn_tree_t *s;
    void       *ptr;
} sym_ptr_t;
htable_t* sym_ptr_htable_init();
bool regexp_assoc_ptr(htable_t *htable, syn_tree_t *s, void *ptr);

typedef struct {
    unsigned short *states;
    void           **targets;
    size_t         num_states;
    size_t         max_targets, num_targets;
} dfa_t;

typedef struct state_node_s {
    struct state_node_s *next;
    list_t              *state;
} state_node_t;

typedef struct {
    uint32_t hash;
    list_t   *state;
    int      val;
} state_int_t;

dfa_t* regexp_to_dfa(regexp_stat *st);
void dfa_free(dfa_t *dfa);
