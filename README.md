# trans

Lexical analyzer generator.

# Usage

```bash
./trans <filename.trans>
```

It generates two files with names filename.h and filename.c.

filename.h contains three function prototypes:

```c
typedef enum { LEX_ERROR = -1, LEX_SUCCESS = 0, LEX_EOF = 1 } lexer_res_t;

lexer_t* lexer_create(const char *filename);
lexer_res_t lexer_next_tok(lexer_t *lex, lexeme_t *m);
void lexer_free(lexer_t *lex);
```

if lexer_next_tok returns LEX_ERROR or LEX_EOF, lexeme will not contain a valid value.

## Usage Example

```c
int main() {
    lexer_t *lex = lexer_create("filename");
    if(!lex)
        return 1;

    for(;;) {
        lexeme_t m;
        lexer_res_t res = lexer_next_tok(lex, &m);
        if(res == LEX_ERROR) {
            // handle error
        } else if(res == LEX_EOF) {
            break;
        } else {
            // valid lexeme value
        }
    }

    lexer_free(lex);
}
```

# Trans file specification

.trans files are used to describe language for which lexical analyzer is created. They have following structure:

```
[sectionname]
section content

[nextsection]
next section content

...
```

There are two necessary sections in any .trans file:

1. header — must contain lexeme_t definition and all definitions related to it. This section entirely includes into result .h file.
2. regexes — must contain regular expressions which defines lexemes. Specification of regular expressions format is written below.

And three not-necessary:

1. hinclude — must contain headers which will be included into result .h file.
2. cinclude — must contain headers which will be included into result .c file.
3. funcs — must contain ancillary functions for parsing lexeme. This section entirely includes into result .c file.

## Header section and lexeme_t specification

Header section must conatain lexeme_t definition in corresponding format:

```c
typedef struct {
    // necessary fields
    int class;
    char *str;
    size_t str_len;
    // not-necessary fields
} lexeme_t;
```

Every lexeme_t definition must contain at least three fields:

1. int class — lexeme class. For example: ID, IF, THEN, etc.
2. char \*str — string corresponding to lexeme. If you need to use this string after lexeme parsing, you'll need to allocate memory and copy this string to it.
3. size_t str_len — length of string corresponding to lexeme.

## Regexes section specification

Regexes section must contains at least one regular expression in following format:

```
"regexp1" { /* some c code */ return integer_value_1; }
"regexp2" { 
    /* some c code */
    return integer_value_2;
}
```

Corresponding to regular expression function will be called if input string matches regular expression. All functions will be written in .c file in following format:

```c
static int func(lexeme_t *lex) {
    // code written in .trans file
}
```

if returned value is lower than zero, it will be considered as error. If it equals to zero, then corresponding to lexeme string will considered as delimeter and parsing will continue. If returned value is greater than zero, it will be consider as lexeme class.

## Regex format specification

Supported special characters:

1. \* — zero or more.
2. | — or.
3. () — brackets.
4. . — any character.
5. \\w — ascii letter. [A-Za-z].
6. \\W — non-letter character.
7. \\d — digit. [0-9].
8. \\D — non-digit character.
9. \\s — space, newline or tab character.
10. \\S — non-space character.
11. \\" — " character.
12. \\\* — \* character.
13. \\| — | character.
14. \\( — ( character.
15. \\) — ) character.
16. \\. — . character.

All other character are considered as usual.

# Example

[example](https://github.com/cyberfined/trans/tree/master/example)
