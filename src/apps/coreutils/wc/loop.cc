#include "loop.h"

static inline int isblank(int c) {
    return c == ' ' || c == '\t';
}

static inline int isspace(int c) {
    return isblank(c) || c == '\v' || c == '\f' || c == '\r' || c == '\n';
}

void count(const char *buffer, size_t res, long *lines, long *words, int *last_space) {
    for(size_t i = 0; i < res; ++i) {
        if(buffer[i] == '\n')
            (*lines)++;
        int space = isspace(buffer[i]);
        if(!*last_space && space)
            (*words)++;
        *last_space = space;
    }
}
