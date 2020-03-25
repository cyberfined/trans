#include "regexp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "llist.h"
#include "htable.h"

static void __print_syn_tree(syn_tree_t *s) {
    if(s->tag == SYM) {
        putchar(s->sym.chr);
    } else if(s->tag == OR) {
        fputs("OR (", stdout);
        __print_syn_tree(s->or.s1);
        fputs(") (", stdout);
        __print_syn_tree(s->or.s2);
        putchar(')');
    } else if(s->tag == AND) {
        fputs("AND (", stdout);
        __print_syn_tree(s->and.s1);
        fputs(") (", stdout);
        __print_syn_tree(s->and.s2);
        putchar(')');
    } else if(s->tag == STAR) {
        fputs("STAR (", stdout);
        __print_syn_tree(s->star.s);
        putchar(')');
    }
}

void print_syn_tree(syn_tree_t *s) {
    __print_syn_tree(s);
    putchar('\n');
}

//static bool all(char c) { return (33 <= c && c <= 126) || c == ' ' || c == '\t' || c == '\n'; }
static bool nothing(char c) { return false; }
static bool all(char c) { return true; }
static bool letter(char c) { return (65 <= c && c <= 90) || (97 <= c && c <= 122); }
static bool not_letter(char c) { return (33 <= c && c <= 64) || (91 <= c && c <= 96) || (123 <= c && c <= 126); }
static bool space(char c) { return c == ' ' || c == '\t' || c == '\n'; }
static bool not_space(char c) { return 33 <= c && c <= 126; }
static bool digit(char c) { return 48 <= c && c <= 57; }
static bool not_digit(char c) { return (33 <= c && c <= 47) || (58 <= c && c <= 126); }

syn_tree_t* parse_sym(const char *regexp, size_t len, size_t *off, bool *error) {
    syn_tree_t *t = NULL;
    size_t v_off;

    *error = false;
    if(len == 0) return NULL;

    if((regexp[0] >= 33 && regexp[0] <= 39)   ||
       (regexp[0] >= 43 && regexp[0] <= 45)   ||
       (regexp[0] >= 47 && regexp[0] <= 91)   ||
       (regexp[0] >= 93 && regexp[0] <= 123)  ||
       (regexp[0] == 125 || regexp[0] == 126) ||
       (len >= 2 && regexp[0] == '\\'))
    {
        t = malloc(sizeof(syn_tree_t));
        if(!t) {
            perror("malloc");
            goto exit;
        }

        t->sym.pred = NULL;
        t->tag = SYM;

        if(regexp[0] == '\\') {
            switch(regexp[1]) {
                case 'w':  t->sym.chr = 'w'; t->sym.pred = letter;     break;
                case 'W':  t->sym.chr = 'W'; t->sym.pred = not_letter; break;
                case 's':  t->sym.chr = 's'; t->sym.pred = space;      break;
                case 'S':  t->sym.chr = 'S'; t->sym.pred = not_space;  break;
                case 'd':  t->sym.chr = 'd'; t->sym.pred = digit;      break;
                case 'D':  t->sym.chr = 'D'; t->sym.pred = not_digit;  break;
                case '\\': case '.': case '*': case '|': case '(': case ')': case '"': t->sym.chr = regexp[1]; break;
                default:
                    fprintf(stderr, "\\%c is unexpected control character\n", regexp[1]);
                    goto exit;
            }
            v_off = 2;
        } else {
            t->sym.chr = regexp[0];
            v_off = 1;
            if(regexp[0] == '.')
                t->sym.pred = all;
        }

        if(len > v_off && regexp[v_off] == '*') {
            if(len > v_off+1 && regexp[v_off+1] == '*') {
                fputs("* is unexpected\n", stderr);
                goto exit;
            }

            syn_tree_t *st = malloc(sizeof(syn_tree_t));
            if(!st) {
                perror("malloc");
                goto exit;
            }
            v_off++;
            st->tag = STAR;
            st->star.s = t;
            t = st;
        }

        *off = v_off;
        return t;
    }

    return NULL;
exit:
    *error = true;
    if(t) free(t);
    return NULL;
}

syn_tree_t* parse_brackets(const char *regexp, size_t len, size_t *off, bool *error) {
    syn_tree_t *t = NULL;
    int nested = 1;
    size_t i;

    *error = false;
    if(len == 0 || regexp[0] != '(') return NULL;

    for(i = 1; i < len; i++) {
        if(regexp[i] == '(') {
            nested++;
        } else if(regexp[i] == ')') {
            nested--;
            if(nested == 0) {
                break;
            } else if(nested < 0) {
                fputs(") is unexpected\n", stderr);
                goto exit;
            }
        }
    }

    if(nested != 0) {
        fputs("( is unclosed\n", stderr);
        goto exit;
    }

    t = parse_regexp(regexp+1, i-1, off, error);
    if(*error || !t)
        goto exit;
    
    i++;
    if(len > i && regexp[i] == '*') {
        if(len > i+1 && regexp[i+1] == '*') {
            fputs("* is unexpected\n", stderr);
            goto exit;
        }

        syn_tree_t *st = malloc(sizeof(syn_tree_t));
        if(!st) {
            perror("malloc");
            goto exit;
        }

        st->tag = STAR;
        st->star.s = t;
        t = st;
        i++;
    }

    *off = i;
    return t;
exit:
    *error = true;
    if(t) syn_tree_free(t);
    return NULL;
}

syn_tree_t* parse_regexp(const char *regexp, size_t len, size_t *off, bool *error) {
    syn_tree_t *t = NULL, *tmp;
    size_t v_off = 0, bak_len = len;

    while(len > 0) {
        tmp = parse_brackets(regexp, len, off, error);
        if(*error)
            goto exit;
        if(!tmp) {
            tmp = parse_sym(regexp, len, off, error);
            if(*error)
                goto exit;
            if(!tmp) {
                *error = true;
                fprintf(stderr, "Failed to parse %s\n", regexp);
                goto exit;
            }
        }
        v_off = *off;
        len -= v_off;
        regexp += v_off;

        if(t != NULL)
            t->and.s2 = tmp;
        else
            t = tmp;

        if(len > 0) {
            syn_tree_t *st = malloc(sizeof(syn_tree_t));
            if(!st) {
                *error = true;
                perror("malloc");
                goto exit;
            }

            if(regexp[0] == '|') {
                regexp++;
                len--;

                st->tag = OR;
                st->or.s1 = t;
                st->or.s2 = parse_regexp(regexp, len, off, error);
                t = st;

                if(*error || !st->or.s2)
                    goto exit;

                v_off = *off;
                regexp += v_off;
                len -= v_off;
            } else {
                st->tag = AND;
                st->and.s1 = t;
                st->and.s2 = NULL;
                t = st;
            }
        }
    }

    *off = bak_len-len;
    return t;
exit:
    if(t) syn_tree_free(t);
    return NULL;
}

void syn_tree_free(syn_tree_t *s) {
    if(s->tag == OR) {
        syn_tree_free(s->or.s1);
        if(s->or.s2) syn_tree_free(s->or.s2);
    } else if(s->tag == AND) {
        syn_tree_free(s->and.s1);
        if(s->and.s2) syn_tree_free(s->and.s2);
    } else if(s->tag == STAR) {
        syn_tree_free(s->star.s);
    }
    free(s);
}

#define MIN_FOLLOWPOS_SIZE 30

static inline sym_node_t* sym_node_create(syn_tree_t *t, size_t ind) {
    sym_node_t *n = malloc(sizeof(sym_node_t));
    if(!n) {
        perror("malloc");
        return NULL;
    }
    n->t = t;
    n->ind = ind;
    return n;
}

static node_t* sym_node_copy(node_t *_src) {
    sym_node_t *src = (sym_node_t*)_src;
    return (node_t*)sym_node_create(src->t, src->ind);
}

static bool get_tree_stat(syn_tree_t *t, regexp_stat *st) {
    bool ns1;
    list_t *fps1, *lps1;

    if(st->num_syms >= st->max_syms && (t->tag == AND || t->tag == STAR)) {
        size_t from = st->max_syms;
        st->max_syms <<= 1;
        list_t **new_follow = realloc(st->followpos, st->max_syms*sizeof(list_t*));
        if(!new_follow) {
            perror("realloc");
            return false;
        }
        st->followpos = new_follow;
        for(size_t i = from; i < st->max_syms; i++)
            st->followpos[i] = NULL;
    }

    if(t->tag == AND) {
        if(!get_tree_stat(t->and.s1, st))
            return false;
        ns1 = st->nullable;
        fps1 = st->firstpos;
        lps1 = st->lastpos;

        if(!get_tree_stat(t->and.s2, st))
            return false;

        list_for_each(i, sym_node_t, lps1) {
            size_t ind = i->ind;
            list_t *cpy = list_copy(st->firstpos);
            if(!cpy)
                return false;
            if(!st->followpos[ind]) {
                st->followpos[ind] = list_create(sym_node_copy, NULL);
                if(!st->followpos[ind]) {
                    list_free(cpy);
                    return false;
                }
            }
            st->followpos[ind] = list_union(st->followpos[ind], cpy);
        }

        if(ns1) {
            st->firstpos = list_union(fps1, st->firstpos);
        } else {
            list_free(st->firstpos);
            st->firstpos = fps1;
        }

        if(st->nullable) {
            st->lastpos = list_union(st->lastpos, lps1);
        } else {
            list_free(lps1);
        }

        st->nullable = ns1 && st->nullable;
    } else if(t->tag == OR) {
        if(!get_tree_stat(t->or.s1, st))
            return false;
        ns1 = st->nullable;
        fps1 = st->firstpos;
        lps1 = st->lastpos;

        if(!get_tree_stat(t->or.s2, st))
            return false;

        st->firstpos = list_union(fps1, st->firstpos);
        st->lastpos = list_union(lps1, st->lastpos);
        st->nullable = st->nullable || ns1;
    } else if(t->tag == SYM) {
        st->firstpos = list_create(sym_node_copy, NULL);
        if(!st->firstpos)
            return false;
        sym_node_t *n = sym_node_create(t, st->num_syms);
        if(!n)
            return false;
        list_append(st->firstpos, n);

        st->lastpos = list_create(sym_node_copy, NULL);
        if(!st->lastpos)
            return false;
        n = sym_node_create(t, st->num_syms);
        if(!n)
            return false;
        list_append(st->lastpos, n);
        st->nullable = false;
        st->num_syms++;
    } else if(t->tag == STAR) {
        if(!get_tree_stat(t->star.s, st))
            return false;

        list_for_each(i, sym_node_t, st->lastpos) {
            size_t ind = i->ind;
            list_t *cpy = list_copy(st->firstpos);
            if(!cpy)
                return false;
            if(!st->followpos[ind]) {
                st->followpos[ind] = list_create(sym_node_copy, NULL);
                if(!st->followpos[ind]) {
                    list_free(cpy);
                    return false;
                }
            }
            st->followpos[ind] = list_union(st->followpos[ind], cpy);
        }

        st->nullable = true;
    } else {
        return false;
    }

    return true;
}

syn_tree_t* regexp_ext(syn_tree_t *s1) {
    syn_tree_t *a = NULL, *s2 = NULL;

    a = malloc(sizeof(syn_tree_t));
    if(!a) {
        perror("malloc");
        goto exit;
    }

    s2 = malloc(sizeof(syn_tree_t));
    if(!s2) {
        perror("malloc");
        goto exit;
    }
    s2->tag = SYM;
    s2->sym.chr = 0;
    s2->sym.pred = nothing;

    a->tag = AND;
    a->and.s1 = s1;
    a->and.s2 = s2;
    return a;
exit:
    if(a) free(a);
    if(s2) free(s2);
    return NULL;
}

static uint32_t sym_ptr_hash(hnode_t *_n) {
    sym_ptr_t *n = (sym_ptr_t*)_n;
    return ((unsigned long)n->s) & 0xffffffff;
}

static bool sym_ptr_keyeq(hnode_t *_n1, hnode_t *_n2) {
    sym_ptr_t *n1 = (sym_ptr_t*)_n1;
    sym_ptr_t *n2 = (sym_ptr_t*)_n2;
    return n1->s == n2->s;
}

regexp_stat* get_regexp_stat(syn_tree_t *ext, htable_t *regexp_ptrs) {
    regexp_stat *st = malloc(sizeof(regexp_stat));
    if(!st) {
        perror("malloc");
        goto exit;
    }

    st->max_syms = MIN_FOLLOWPOS_SIZE;
    st->num_syms = 0;
    st->regexp_ptrs = regexp_ptrs;
    st->followpos = calloc(st->max_syms, sizeof(list_t*));
    if(!st->followpos) {
        perror("calloc");
        goto exit;
    }

    syn_tree_t *s1 = ext->and.s1;
    syn_tree_t *s2 = ext->and.s2;
    if(!get_tree_stat(s1, st))
        goto exit;

    if(st->max_syms < st->num_syms) {
        size_t from = st->max_syms, to = st->num_syms;
        st->max_syms = st->num_syms;
        list_t **tmp = realloc(st->followpos, sizeof(list_t*)*st->max_syms);
        if(!tmp) {
            perror("realloc");
            goto exit;
        }
        st->followpos = tmp;
        for( ; from < to; from++)
            st->followpos[from] = NULL;
    }

    size_t end_ind = st->num_syms;
    void *prev_target = NULL, *cur_target;
    sym_ptr_t key_node, *found_node;
    list_for_each(i, sym_node_t, st->lastpos) {
        size_t ind = i->ind;

        key_node.s = i->t;
        found_node = (sym_ptr_t*)htable_lookup(regexp_ptrs, (hnode_t*)&key_node);
        if(!found_node)
            goto exit;
        cur_target = found_node->ptr;

        if(prev_target && prev_target != cur_target)
            end_ind++;

        sym_node_t *newnode = sym_node_create(s2, end_ind);
        if(!newnode)
            goto exit;
        if(!st->followpos[ind]) {
            st->followpos[ind] = list_create(sym_node_copy, NULL);
            if(!st->followpos[ind]) {
                free(newnode);
                goto exit;
            }
        }
        list_append(st->followpos[ind], newnode);
        prev_target = cur_target;
    }

    return st;
exit:
    free_regexp_stat(st);
    return NULL;
}

void free_regexp_stat(regexp_stat *st) {
    if(st->firstpos) list_free(st->firstpos);
    if(st->lastpos) list_free(st->lastpos);
    if(st->followpos) {
        for(size_t i = 0; i < st->max_syms; i++)
            if(st->followpos[i])
                list_free(st->followpos[i]);
        if(st->followpos) free(st->followpos);
    }
    free(st);
}

htable_t* sym_ptr_htable_init() {
    return htable_create(sizeof(sym_ptr_t), 8, sym_ptr_hash, sym_ptr_keyeq, NULL);
}

bool regexp_assoc_ptr(htable_t *htable, syn_tree_t *s, void *ptr) {
    if(s->tag == AND) {
        return regexp_assoc_ptr(htable, s->and.s1, ptr) && regexp_assoc_ptr(htable, s->and.s2, ptr);
    } else if(s->tag == OR) {
        return regexp_assoc_ptr(htable, s->or.s1, ptr) && regexp_assoc_ptr(htable, s->or.s2, ptr);
    } else if(s->tag == STAR) {
        return regexp_assoc_ptr(htable, s->star.s, ptr);
    } else if(s->tag == SYM) {
        sym_ptr_t key_node;
        key_node.s = s;
        key_node.ptr = ptr;
        return htable_insert(htable, (hnode_t*)&key_node);
    }
    return false;
}

static inline state_node_t* state_node_create(list_t *state) {
    state_node_t *n = malloc(sizeof(state_node_t));
    if(!n) {
        perror("malloc");
        return NULL;
    }
    n->state = state;
    return n;
}

static void state_node_free(node_t *_n) {
    state_node_t *n = (state_node_t*)_n;
    list_free(n->state);
}

static uint32_t state_int_hash(hnode_t *_n) {
    state_int_t *n = (state_int_t*)_n;
    uint32_t hash = 0;
    uint32_t base = 1;
    list_for_each(i, sym_node_t, n->state) {
        hash += i->ind * base;
        base *= 10;
    }
    return hash;
}

static bool state_int_keyeq(hnode_t *_n1, hnode_t *_n2) {
    state_node_t *n1 = (state_node_t*)_n1;
    state_node_t *n2 = (state_node_t*)_n2;
    sym_node_t *i = (sym_node_t*)n1->state->first;
    sym_node_t *j = (sym_node_t*)n2->state->first;
    bool is_eq = true;

    while(i != NULL || j != NULL) {
        if(((i == NULL || j == NULL) && i != j) || i->ind != j->ind) {
            is_eq = false;
            break;
        }
        i = i->next;
        j = j->next;
    }

    return is_eq;
}

static inline bool dfa_target_insert(dfa_t *dfa, void *target) {
    if(dfa->num_targets == dfa->max_targets) {
        dfa->max_targets <<= 1;
        void **newtargets = realloc(dfa->targets, sizeof(void*)*dfa->max_targets);
        if(!newtargets) {
            perror("realloc");
            return false;
        }
        dfa->targets = newtargets;
    }
    dfa->targets[dfa->num_targets++] = target;
    return true;
}

dfa_t* regexp_to_dfa(regexp_stat *st) {
    dfa_t *dfa = NULL;
    list_t *states = NULL;
    htable_t *htable = NULL;
    state_int_t st_key_node, *st_found_node;
    sym_ptr_t pt_key_node, *pt_found_node;
    int cur_state_ind = 0;
    int new_state_ind = 1;
    int state_num;
    int end_state = st->num_syms;
    size_t states_bytes;

    list_t *cpy, *newstate = NULL;
    state_node_t *newnode;

    void *new_target;
    bool prev_pred;

    // init dfa
    dfa = calloc(1, sizeof(dfa_t));
    if(!dfa) {
        perror("calloc");
        goto exit;
    }
    states_bytes = sizeof(short) * (st->max_syms<<1) * 255;
    dfa->states = malloc(states_bytes);
    if(!dfa->states) {
        perror("malloc");
        goto exit;
    }
    memset(dfa->states, 0, states_bytes);

    // create states list
    states = list_create(NULL, state_node_free);
    if(!states)
        goto exit;
    cpy = list_copy(st->firstpos);
    if(!cpy)
        goto exit;
    newnode = state_node_create(cpy);
    if(!newnode)
        goto exit;
    list_append(states, newnode);

    // create targets array
    dfa->max_targets = 8;
    dfa->num_targets = 1;
    dfa->targets = malloc(sizeof(void*)*dfa->max_targets);
    if(!dfa->targets) {
        perror("malloc");
        goto exit;
    }
    dfa->targets[0] = NULL;

    // init htable
    htable = htable_create(sizeof(state_int_t), st->num_syms, state_int_hash, state_int_keyeq, NULL);
    if(!htable)
        goto exit;

    state_node_t *cur_state_node = (state_node_t*)states->first;
    for(;;) {
        if(!cur_state_node)
            break;

        for(int c = 0; c <= 255; c++) {
            if(!newstate) {
                newstate = list_create(sym_node_copy, NULL);
                if(!newstate)
                    goto exit;
            }
            new_target = NULL;
            prev_pred = false;
            list_for_each(i, sym_node_t, cur_state_node->state) {
                if((!i->t->sym.pred && i->t->sym.chr != c) ||
                   (i->t->sym.pred && !i->t->sym.pred(c)))
                    continue;
                
                cpy = list_copy(st->followpos[i->ind]);
                if(!cpy)
                    goto exit;

                if(!new_target || (prev_pred && !i->t->sym.pred)) {
                    prev_pred = i->t->sym.pred != NULL;
                    list_for_each(j, sym_node_t, cpy) {
                        if(j->ind >= end_state) {
                            pt_key_node.s = i->t;
                            pt_found_node = (sym_ptr_t*)htable_lookup(st->regexp_ptrs, (hnode_t*)&pt_key_node);
                            if(!pt_found_node)
                                goto exit;
                            new_target = pt_found_node->ptr;
                            break;
                        }
                    }
                }

                newstate = list_union(newstate, cpy);
            }
            if(!newstate->first) {
                if(c == 255) {
                    list_free(newstate);
                    newstate = NULL;
                }
                continue;
            }
            st_key_node.state = newstate;
            st_key_node.hash = state_int_hash((hnode_t*)&st_key_node);
            st_found_node = (state_int_t*)htable_lookup(htable, (hnode_t*)&st_key_node);
            if(!st_found_node) {
                // append state into targets if it contains end_state
                if(!dfa_target_insert(dfa, new_target))
                    goto exit;
                newnode = state_node_create(newstate);
                if(!newnode)
                    goto exit;
                list_append(states, newnode);
                st_key_node.val = new_state_ind;
                if(!htable_insert(htable, (hnode_t*)&st_key_node))
                    goto exit;
                state_num = new_state_ind;
                new_state_ind++;
            } else {
                state_num = st_found_node->val;
                list_free(newstate);
            }
            dfa->states[cur_state_ind*255 + c] = state_num;
            newstate = NULL;
        }

        cur_state_node = cur_state_node->next;
        cur_state_ind++;
    }

    dfa->num_states = cur_state_ind;
    list_free(states);
    htable_free(htable);
    return dfa;
exit:
    if(dfa) {
        if(dfa->states) free(dfa->states);
        if(dfa->targets) free(dfa->targets);
        free(dfa);
    }
    if(newstate) list_free(newstate);
    if(states) list_free(states);
    if(htable) htable_free(htable);
    return NULL;
}

void dfa_free(dfa_t *dfa) {
    free(dfa->states);
    free(dfa->targets);
    free(dfa);
}
