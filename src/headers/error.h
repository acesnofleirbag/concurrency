#ifndef HEADERS_ERROR_H
#define HEADERS_ERROR_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void
errlog(char* fmt, ...) {
    va_list args;

    fprintf(stderr, "%s:%d: ", __FILE__, __LINE__);
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");

    exit(EXIT_FAILURE);
}

#endif
