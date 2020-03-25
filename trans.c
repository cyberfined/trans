#include "trans.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>

static uint32_t unit_node_hash(hnode_t *_n) {
    unit_node_t *n = (unit_node_t*)_n;
    return default_hash_func((uint8_t*)n->title, n->title_len);
}

static bool unit_node_keyeq(hnode_t *_n1, hnode_t *_n2) {
    unit_node_t *n1 = (unit_node_t*)_n1;
    unit_node_t *n2 = (unit_node_t*)_n2;
    return !strcmp(n1->title, n2->title);
}

static void unit_node_free(hnode_t *_n) {
    unit_node_t *n = (unit_node_t*)_n;
    free(n->title);
    free(n->content);
}

htable_t* parse_trans_file(const char *filename) {
    htable_t *htable = NULL;
    unit_node_t key_node;
    int fd = -1;
    char buf[4096], *bufptr, *title = NULL, *content = NULL;
    size_t max_bytes = 0, cur_bytes = 0;

    htable = htable_create(sizeof(unit_node_t), 3, unit_node_hash, unit_node_keyeq, unit_node_free);
    if(!htable)
        goto exit;

    fd = open(filename, O_RDONLY);
    if(fd < 0) {
        perror("open");
        goto exit;
    }

    ssize_t nbytes, res_len;
    bool title_reading = false, prev_nl = false;
    do {
        nbytes = read(fd, buf, sizeof(buf));
        if(nbytes < 0) {
            perror("read");
            goto exit;
        }
        bufptr=buf;
        res_len=nbytes;

        while(res_len > 0) {
            if(!title) {
                if(bufptr[0] != '[') {
                    fputs("Failed to read title, expected [\n", stderr);
                    goto exit;
                }
                max_bytes = 8;
                cur_bytes = 0;
                title = malloc(max_bytes);
                if(!title) {
                    perror("malloc");
                    goto exit;
                }
                title_reading = true;
                bufptr++; res_len--;
            } else if(!content && !title_reading) {
                if(bufptr[0] != '\n') {
                    fputs("Expected \\n after title\n", stderr);
                    goto exit;
                }
                max_bytes = 8;
                cur_bytes = 0;
                content = malloc(max_bytes);
                if(!content) {
                    perror("malloc");
                    goto exit;
                }
                bufptr++; res_len--;
            }

            for( ; res_len >= 0; res_len--) {
                char c;
                if(res_len > 0)
                    c = *bufptr++;
                else
                    c = 0;
                char *rb, **rbptr;
                if(title_reading) {
                    rb = title;
                    rbptr = &title;
                    if(c == ']') {
                        c = 0;
                        title_reading = false;
                    } else if(c < 'A' || (c > 'Z' && c < 'a') || c > 'z') {
                        fprintf(stderr, "%c is illegal character in title\n", c);
                        goto exit;
                    } 
                } else {
                    rb = content;
                    rbptr = &content;
                    if(c == '[' && prev_nl) {
                        c = 0;
                        bufptr--; res_len++;
                    } else if((c < 33 || c >= 127) && c != ' ' && c != '\n' && c != '\t' && c != '\r' && res_len != 0) {
                        fprintf(stderr, "\\x%02x is illegal character in content\n", (unsigned char)c);
                        goto exit;
                    }
                }

                if(cur_bytes == max_bytes) {
                    max_bytes <<= 1;
                    char *tmp = realloc(rb, max_bytes);
                    if(!tmp) {
                        perror("realloc");
                        goto exit;
                    }
                    *rbptr = tmp;
                    rb = tmp;
                }
                rb[cur_bytes++] = c;
                if(c == 0) {
                    if(content) {
                        key_node.content = content;
                        key_node.content_len = cur_bytes-1;
                        if(!htable_insert(htable, (hnode_t*)&key_node))
                            goto exit;
                        title = content = NULL;
                    } else {
                        key_node.title = title;
                        key_node.title_len = cur_bytes-1;
                    }
                    res_len--;
                    break;
                }
                prev_nl = c == '\n';
            }
        }
    } while(nbytes == sizeof(buf));

    close(fd);
    return htable;
exit:
    if(htable) htable_free(htable);
    if(fd >= 0) close(fd);
    if(title) free(title);
    if(content) free(content);
    return NULL;
}

bool check_trans_units(htable_t *htable) {
    unit_node_t key_node;

    key_node.title = "header";
    key_node.title_len = 6;
    if(!htable_lookup(htable, (hnode_t*)&key_node)) {
        fputs("header unit doesn't exist\n", stderr);
        return false;
    }

    key_node.title = "regexes";
    key_node.title_len = 7;
    if(!htable_lookup(htable, (hnode_t*)&key_node)) {
        fputs("regexes unit doesn't exist\n", stderr);
        return false;
    }

    return true;
}

regexp_func_t* parse_regexes(const char *str, size_t len, size_t *_num_funcs) {
    size_t num_funcs=0, max_funcs = 8, cur_bytes, max_bytes;
    char *regexp = NULL, *func = NULL, *rb, **rbptr;
    regexp_func_t *funcs = NULL;
    bool regex_reading;
    size_t regexp_len;
    int nested;

    funcs = malloc(sizeof(regexp_func_t)*max_funcs);
    if(!funcs) {
        perror("malloc");
        goto exit;
    }

    char prev = 0;
    for( ; len > 0; len--) {
        if(!regexp) {
            while(len > 0 && (*str == '\n' || *str == '\r')) {
                str++;
                len--;
            }
            if(len == 0)
                break;
            if(str[0] != '"') {
                fprintf(stderr, "Expected \" before regexp, but get '%c'\n", str[0]);
                goto exit;
            }
            max_bytes = 8;
            cur_bytes = 0;
            regexp = malloc(max_bytes);
            if(!regexp) {
                perror("malloc");
                goto exit;
            }
            regex_reading = true;
            str++; len--;
        } else if(!func && !regex_reading) {
            while(len > 0 && (*str == ' ' || *str == '\n' || *str == '\t' || *str == '\r')) {
                str++;
                len--;
            }
            if(len == 0) {
                fputs("Unexpected end of file: expected function after regexp\n", stderr);
                goto exit;
            }
            if(*str != '{') {
                fprintf(stderr, "Expected { after regexp, but get '%c'\n", *str);
                goto exit;
            }
            max_bytes = 8;
            cur_bytes = 0;
            func = malloc(max_bytes);
            if(!func) {
                perror("malloc");
                goto exit;
            }
            nested = 0;
        }

        char c = *str++;
        if(regex_reading) {
            rb = regexp;
            rbptr = &regexp;
            if(c == '"' && prev != '\\') {
                c = 0;
                regex_reading = false;
            }
        } else {
            rb = func;
            rbptr = &func;
            if(c == '{')
                nested++;
            else if(c == '}')
                nested--;
            if(prev == '}' && nested == 0) {
                str--; len++;
                c = 0;
            }
        }

        if(cur_bytes == max_bytes) {
            max_bytes <<= 1;
            char *tmp = realloc(rb, max_bytes);
            if(!tmp) {
                perror("realloc");
                goto exit;
            }
            *rbptr = tmp;
            rb = tmp;
        }
        rb[cur_bytes++] = c;
        prev = c;

        if(c == 0) {
            if(func) {
                if(num_funcs == max_funcs) {
                    max_funcs <<= 1;
                    regexp_func_t *tmp = realloc(funcs, max_funcs*sizeof(regexp_func_t));
                    if(!tmp) {
                        perror("realloc");
                        goto exit;
                    }
                    funcs = tmp;
                }
                funcs[num_funcs++] = (regexp_func_t) {
                    .regexp = regexp,
                    .func = func,
                    .regexp_len = regexp_len,
                    .func_len = cur_bytes-1,
                };
                func = regexp = NULL;
            } else {
                regexp_len = cur_bytes-1;
            }
        }
    }

    *_num_funcs = num_funcs;
    return funcs;
exit:
    if(funcs) {
        for(size_t i = 0; i < num_funcs; i++) {
            free(funcs[i].regexp);
            free(funcs[i].func);
        }
        free(funcs);
    }
    if(regexp) free(regexp);
    if(func) free(func);
    return NULL;
}
