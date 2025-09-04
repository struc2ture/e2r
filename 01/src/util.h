#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define fatal(FMT, ...) do { \
    fprintf(stderr, "[FATAL: %s:%d:%s]: " FMT "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
    abort(); \
} while (0)

#define warning(FMT, ...) do { \
    printf("[WARNING: %s:%d:%s]: " FMT "\n",  __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
} while (0)

#define trace(FMT, ...) do { \
    printf("[TRACE: %s:%d:%s]: " FMT "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
} while (0)

#undef assert
#define assert(cond) do { \
    if (!(cond)) { \
        abort(); \
    } \
} while (0)

#define bp() __builtin_debugtrap()

#define array_count(ARRAY) (sizeof(ARRAY)/sizeof(ARRAY[0]))

#define noop() do {} while (0)

static void *xmalloc(size_t size)
{
    void *ptr = malloc(size);
    if (!ptr) fatal("malloc failed for %zu", size);
    return ptr;
}

static void *xcalloc(size_t size)
{
    void *ptr = calloc(1, size);
    if (!ptr) fatal("calloc failed for %zu", size);
    return ptr;
}

static char *strf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    size_t size = vsnprintf(NULL, 0, fmt, args) + 1;
    va_end(args);
    char *str = xmalloc(size);
    va_start(args, fmt);
    vsnprintf(str, size, fmt, args);
    va_end(args);
    return str;
}
