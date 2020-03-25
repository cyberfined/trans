#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include "htable.h"
#include "regexp.h"
#include "trans.h"
#include "lexer.h"

static inline bool get_output_names(char *origin, char **head_file, char **src_file) {
    size_t len = strlen(origin);
    size_t i;
    for(i = len-1; i > 0; i--) {
        if(origin[i] == '.')
            break;
    }
    if(i != 0)
        len = i+1;

    char *hdr = malloc(len + 2);
    if(!hdr) {
        perror("malloc");
        return false;
    }

    char *src = malloc(len + 2);
    if(!src) {
        perror("malloc");
        free(hdr);
        return false;
    }

    memcpy(hdr, origin, len);
    memcpy(hdr+len, "h", 2);
    memcpy(src, origin, len);
    memcpy(src+len, "c", 2);
    *head_file = hdr;
    *src_file = src;

    return true;
}

static inline bool gen_h_file(const char *filename, htable_t *trans_units) {
    FILE *fd = fopen(filename, "w");
    if(!fd) {
        perror("fopen");
        return false;
    }

    unit_node_t key_node, *header_node, *include_node;
    key_node.title = "header";
    key_node.title_len = 6;
    header_node = (unit_node_t*)htable_lookup(trans_units, (hnode_t*)&key_node);

    key_node.title = "hinclude";
    key_node.title_len = 8;
    include_node = (unit_node_t*)htable_lookup(trans_units, (hnode_t*)&key_node);
    
    fputs("#pragma once\n", fd);
    for(int i = 0; i < sizeof(lexer_h_headers)/sizeof(*lexer_h_headers); i++) {
        if(!include_node || !strstr(include_node->content, lexer_h_headers[i]))
            fprintf(fd, "#include <%s>\n", lexer_h_headers[i]);
    }
    if(include_node)
        fputs(include_node->content, fd);

    fprintf(fd, "\n%s", header_node->content);
    fputs(lexer_h, fd);
    fclose(fd);
    return true;
}

static inline bool gen_c_file(const char *filename, char *hdr_name, htable_t *trans_units, regexp_func_t *funcs, size_t num_funcs, dfa_t *dfa) {
    FILE *fd = fopen(filename, "w");
    if(!fd) {
        perror("fopen");
        return false;
    }

    char *rel_hdr = strrchr(hdr_name, '/');
    if(!rel_hdr)
        rel_hdr = hdr_name;
    else
        rel_hdr++;
    fprintf(fd, "#include \"%s\"\n\n", rel_hdr);

    unit_node_t key_node, *funcs_node, *include_node;
    key_node.title = "cinclude";
    key_node.title_len = 8;
    include_node = (unit_node_t*)htable_lookup(trans_units, (hnode_t*)&key_node);
    for(int i = 0; i < sizeof(lexer_c_headers)/sizeof(*lexer_c_headers); i++) {
        if(!include_node || !strstr(include_node->content, lexer_c_headers[i]))
            fprintf(fd, "#include <%s>\n", lexer_c_headers[i]);
    }
    if(include_node)
        fputs(include_node->content, fd);

    key_node.title = "funcs";
    key_node.title_len = 5;
    funcs_node = (unit_node_t*)htable_lookup(trans_units, (hnode_t*)&key_node);
    if(funcs_node)
        fprintf(fd, "%s\n", funcs_node->content);

    for(size_t i = 0; i < num_funcs; i++)
        fprintf(fd, "static int f%lu(lexeme_t *lex) %s\n", i, funcs[i].func);

    if(dfa->num_states <= 255)
        fputs("\nstatic unsigned char states[] = { ", fd);
    else
        fputs("\nstatic unsigned short states[] = { ", fd);

    size_t num_states = dfa->num_states*255-1;
    for(size_t i = 0; i < num_states; i++)
        fprintf(fd, "%d, ", dfa->states[i]);
    fprintf(fd, "%d };\n", dfa->states[num_states]);

    fputs("\nstatic int (*targets[])(lexeme_t*) = { ", fd);
    for(size_t i = 0; i < dfa->num_targets; i++) {
        if(!dfa->targets[i]) {
            fputs("NULL", fd);
        } else {
            size_t fnum = (dfa->targets[i]-(void*)funcs)/sizeof(*funcs);
            fprintf(fd, "f%lu", fnum);
        }

        if(i+1 < dfa->num_targets)
            fputs(", ", fd);
    }
    fputs(" };\n\n", fd);
    fputs(lexer_c, fd);

    fclose(fd);
    return true;
}

int main(int argc, char **argv) {
    int ret = 1;
    char *head_file = NULL, *src_file = NULL;
    htable_t *trans_units = NULL;
    unit_node_t un_key_node, *un_found_node;
    regexp_func_t *regexp_funcs = NULL;
    size_t num_regexp_funcs = 0;
    htable_t *regexp_ptrs = NULL;
    syn_tree_t *root = NULL, *cur, *tmp, **rootptr = &root;
    size_t off;
    bool error;
    regexp_stat *st = NULL;
    dfa_t *dfa = NULL;

    if(argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        goto exit;
    }

    if(!get_output_names(argv[1], &head_file, &src_file))
        goto exit;

    if(access(head_file, F_OK) == 0) {
        fprintf(stderr, "Can't create file, %s already exist\n", head_file);
        goto exit;
    }

    if(access(src_file, F_OK) == 0) {
        fprintf(stderr, "Can't create file, %s already exist\n", src_file);
        goto exit;
    }

    trans_units = parse_trans_file(argv[1]);
    if(!trans_units)
        goto exit;

    if(!check_trans_units(trans_units))
        goto exit;

    un_key_node.title = "regexes";
    un_key_node.title_len = 7;
    un_found_node = (unit_node_t*)htable_lookup(trans_units, (hnode_t*)&un_key_node);
    regexp_funcs = parse_regexes(un_found_node->content, un_found_node->content_len, &num_regexp_funcs);
    if(!regexp_funcs)
        goto exit;

    if(num_regexp_funcs == 0) {
        fputs("There is must be at least one regexp\n", stderr);
        goto exit;
    } 

    regexp_ptrs = sym_ptr_htable_init();
    if(!regexp_ptrs)
        goto exit;

    for(size_t i = 0; i < num_regexp_funcs; i++) {
        cur = parse_regexp(regexp_funcs[i].regexp, regexp_funcs[i].regexp_len, &off, &error);
        if(error || !cur)
            goto exit;

        if(i+1 < num_regexp_funcs) {
            tmp = malloc(sizeof(syn_tree_t));
            if(!tmp) {
                perror("malloc");
                goto exit;
            }
            tmp->tag = OR;
            tmp->or.s1 = cur;
        } else {
            tmp = cur;
        }

        *rootptr = tmp;
        rootptr = &tmp->or.s2;

        if(!regexp_assoc_ptr(regexp_ptrs, cur, &regexp_funcs[i]))
            goto exit;
    }

    tmp = regexp_ext(root);
    if(!tmp)
        goto exit;
    root = tmp;

    st = get_regexp_stat(root, regexp_ptrs);
    if(!st)
        goto exit;

    dfa = regexp_to_dfa(st);
    if(!dfa)
        goto exit;

    if(!gen_h_file(head_file, trans_units))
        goto exit;

    if(!gen_c_file(src_file, head_file, trans_units, regexp_funcs, num_regexp_funcs, dfa))
        goto exit;

    ret = 0;
exit:
    if(head_file) free(head_file);
    if(src_file) free(src_file);
    if(trans_units) htable_free(trans_units);
    if(regexp_funcs) {
        for(size_t i = 0; i < num_regexp_funcs; i++) {
            free(regexp_funcs[i].regexp);
            free(regexp_funcs[i].func);
        }
        free(regexp_funcs);
    }
    if(regexp_ptrs) htable_free(regexp_ptrs);
    if(root) syn_tree_free(root);
    if(st) free_regexp_stat(st);
    if(dfa) dfa_free(dfa);
    return ret;
}
