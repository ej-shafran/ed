#ifndef GETLINE_H_
#define GETLINE_H_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>

ssize_t getline(char **lineptr, size_t *n, FILE *stream);

#endif // GETLINE_H_
