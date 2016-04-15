#ifndef POLY_UTILITY_H
#define POLY_UTILITY_H

#include <stdarg.h>

//#define DEBUG

void _quit(const char *fmt, ...)
    __attribute__((format (printf, 1, 2)));

#define quit(fmt, ...) _quit(fmt"\n", ##__VA_ARGS__)

void _plog(const char *fmt, ...)
    __attribute__((format (printf, 1, 2)));

#define plog(fmt, ...) _plog(fmt"\n", ##__VA_ARGS__)

#endif