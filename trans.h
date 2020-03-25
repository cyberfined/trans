#pragma once

#include <stddef.h>
#include <stdint.h>
#include "htable.h"

typedef struct {
    uint32_t hash;
    char     *title;
    char     *content;
    size_t   title_len;
    size_t   content_len;
} unit_node_t;

htable_t* parse_trans_file(const char *filename);
bool check_trans_units(htable_t *htable);

typedef struct {
    char *regexp;
    char *func;
    size_t regexp_len;
    size_t func_len;
} regexp_func_t;

regexp_func_t* parse_regexes(const char *str, size_t len, size_t *_num_funcs);
