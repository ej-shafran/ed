#ifndef LB_H_
#define LB_H_

#include <stdio.h>
#include <stdbool.h>
#include "../da.h"

// Dynamic array of lines (nul-terminated with the '\n' at the end)
typedef da(char *) Line_Builder;

// Read lines from `stream` into `lb` until `condition` is met.
// Passing `""` as the condition will read until EOF.
bool lb_read_from_stream(Line_Builder *lb, FILE *file, char *condition);

// Insert the contents of `source` into `target` at `index`,
// pushing off the contents of `target` to make room.
void lb_insert(Line_Builder *target, Line_Builder *source, size_t index);

// Insert the contents of `source` into `target` at `index`,
// while removing the line at `index` from the target.
void lb_overwrite(Line_Builder *target, Line_Builder *source, size_t start,
		  size_t end);

// Remove the line at `index` from `target`.
void lb_pop_line(Line_Builder *target, size_t index);

// Remove the lines between `start` and `end` from `target`.
void lb_pop_range(Line_Builder *target, size_t start, size_t end);

// Append a line to a `Line_Builder`
#define lb_append(lb, line) da_append(lb, line)

// Iterate over a `Line_Builder`'s lines by pointer
#define lb_foreach(line, lb) da_foreach(line, lb)

// Write all lines from `lb` into `stream`.
#define lb_write_to_stream(lb, stream)        \
	lb_foreach(line, lb)                  \
	{                                     \
		if (fputs(*line, stream) < 0) \
			break;                \
	}

// Wrapper around `lb_read_from_stream` that reads lines from STDIN
// until a line with just `"."` is encountered.
#define lb_read_to_dot(lb) lb_read_from_stream(lb, stdin, ".\n")

// Wrapper around `lb_read_from_stream` that reads lines from `file` until EOF.
#define lb_read_file(lb, file) lb_read_from_stream(lb, file, "")

// Print lines from `lb` into `STDOUT`
#define lb_print(lb, start, end)                        \
	do {                                            \
		for (size_t i = 0; i < lb.count; ++i) { \
			printf("%s", lb.items[i]);      \
		}                                       \
	} while (0);

// Print line numbers from `lb` into `STDOUT`
#define lb_num(lb, start, end)                          \
	do {                                            \
		for (size_t i = start - 1; i < end; ++i) { \
			printf("%zu\n", i + 1);             \
		}                                       \
	} while (0);

// Print lines and their line numbers from `lb` into `STDOUT`
#define lb_printn(lb, start, end)                             \
	do {                                                  \
		for (size_t i = start - 1; i < end; ++i) {       \
			printf("%zu     %s", i + 1, lb.items[i]); \
		}                                             \
	} while (0);

// Free the allocated pointers in `lb`.
#define lb_free(lb)                     \
	do {                            \
		da_foreach(line, lb)    \
		{                       \
			free(*line);    \
		}                       \
		if (lb.items != NULL)   \
			free(lb.items); \
	} while (0);

#define lb_clear(lb)             \
	do {                     \
		lb_free(lb);     \
		lb.count = 0;    \
		lb.capacity = 0; \
		lb.items = NULL; \
	} while (0);

#define realloc_chunk(target, amount)                                       \
	do {                                                                \
		if (target->count + amount > target->capacity) {            \
			if (target->capacity == 0)                          \
				target->capacity = DA_CAP_INIT;             \
			while (target->count + amount > target->capacity) { \
				target->capacity *= 2;                      \
			}                                                   \
			target->items = realloc(                            \
				target->items,                              \
				target->capacity * sizeof(*target->items)); \
			assert(target->items != NULL &&                     \
			       "Could not reallocate memory");              \
		}                                                           \
	} while (0);


#endif // LB_H_
