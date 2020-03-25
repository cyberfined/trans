#pragma once
#include <stddef.h>


typedef enum { NONE=0, ID, NUM, INT, FLOAT, IF, THEN, ELSE, RELOP, EQ, NE, LE, GE, LT, GT } lex_class_t;

typedef struct {
    int class;
    char *str;
    size_t str_len;
    union {
        struct {
            int tag;
            union {
                int i;
                float f;
            };
        } num;
        struct {
            int op;
        } relop;
    };
} lexeme_t;

typedef struct {
    int fd;
    int cur_buf;
    char buf[8192];
    char *symtab;
    size_t max_bytes, num_bytes;
    size_t buf_off, read_len;
    size_t cur_line, cur_chr;
} lexer_t;

typedef enum { LEX_ERROR = -1, LEX_SUCCESS = 0, LEX_EOF = 1 } lexer_res_t;

lexer_t* lexer_create(const char *filename);
lexer_res_t lexer_next_tok(lexer_t *lex, lexeme_t *m);
void lexer_free(lexer_t *lex);
