#include <stdio.h>
#include <stdlib.h>
#include "lang.h"

static inline void print_lexeme(lexeme_t *lex) {
    switch(lex->class) {
        case ID: printf("<ID %s>\n", lex->str); break;
        case NUM:
            switch(lex->num.tag) {
                case INT:   printf("<INT %d>\n", lex->num.i);   break;
                case FLOAT: printf("<FLOAT %f>\n", lex->num.f); break;
            }
            break;
        case IF:   puts("<IF>");   break;
        case THEN: puts("<THEN>"); break;
        case ELSE: puts("<ELSE>"); break;
        case RELOP:
            switch(lex->relop.op) {
                case GT: puts("<GT>"); break;
                case LT: puts("<LT>"); break;
                case GE: puts("<GE>"); break;
                case LE: puts("<LE>"); break;
                case EQ: puts("<EQ>"); break;
                case NE: puts("<NE>"); break;
            }
            break;
    }
}

int main() {
    int res = 0;

    lexer_t *lex = lexer_create("test");
    if(!lex)
        return 1;

    for(;;) {
        lexeme_t m;
        lexer_res_t res = lexer_next_tok(lex, &m);
        if(res == LEX_SUCCESS) {
            print_lexeme(&m);
            if(m.class == ID)
                free(m.str);
        } else if(res == LEX_EOF) {
            break;
        } else {
            res = 1;
            break;
        }
    }

    lexer_free(lex);
    return res;
}
