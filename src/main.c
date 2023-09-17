#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>

#include "../da.h"

typedef da(char *) Line_Builder;

#define lb_free(lb)                  \
	do {                         \
		da_foreach(line, lb) \
		{                    \
			free(*line); \
		}                    \
	} while (0);

#define lb_print(lb)                         \
	do {                                 \
		da_foreach(line, lb)         \
		{                            \
			printf("%s", *line); \
		}                            \
	} while (0);

#define lb_print_n(lb)                                   \
	do {                                             \
		int i = 0;                               \
		da_foreach(line, lb)                     \
		{                                        \
			printf("%d     %s", (i++) + 1, *line); \
		}                                        \
	} while (0);

typedef struct {
	Line_Builder buffer;
	size_t line;
} Ed_Context;

static Ed_Context ed_global_context = {
	.buffer = { 0 },
	.line = 0,
};

bool read_to_lb(Line_Builder *lb, FILE *file, char *condition)
{
	while (true) {
		char *line = NULL;
		size_t nsize = 0;
		ssize_t nread = getline(&line, &nsize, file);

		if (strcmp(condition, line) == 0)
			return true;

		if (nread < 0) {
			free(line);
			return false;
		}

		da_append(lb, line);
	}
}

#define read_to_dot(lb) read_to_lb(lb, stdin, ".\n")

#define read_file(lb, file) read_to_lb(lb, file, "")

/*
 * `insert_at`
 *
 * Insert the contents of `source` into `target` at index `location`,
 * pushing the contents of `target` to make room.
 *
 * `source` is emptied so it cannot be used again.
 * Only `target` should be `free`d afterwards.
 *
 * For example:
 * target = ["hello", "world", "you", "today?"];
 * source = ["how", "are"];
 * insert_at(&target, &source, 2);
 *
 * // results in
 * target = ["hello", "world", "how", "are", "you", "today?"]
 */
void insert_at(Line_Builder *target, Line_Builder *source, size_t location)
{
	assert(location <= target->count);

	if (target->count + source->count > target->capacity) {
		if (target->capacity == 0) {
			target->capacity = DA_CAP_INIT;
		}
		while (target->count + source->count > target->capacity) {
			target->capacity *= 2;
		}
		target->items =
			realloc(target->items,
				target->capacity * sizeof(*target->items));
		assert(target->items != NULL && "Could not reallocate memory");
	}

	memmove(target->items + location + source->count,
		target->items + location,
		(target->count - location) * sizeof(*target->items));
	target->count += source->count;

	memcpy(target->items + location, source->items,
	       source->count * sizeof(source->items));

	source->items = NULL;
	source->count = 0;
	source->capacity = 0;
}

int main(void)
{
	Ed_Context *context = &ed_global_context;
	FILE *in = fopen("./src/main.c", "r");

	if (in == NULL) {
		fprintf(stderr, "Failed to open input file: %s\n",
			strerror(errno));
		return 1;
	}

	bool result = read_file(&context->buffer, in);

	if (!result) {
		fprintf(stderr, "Failed to read input file: %s\n",
			strerror(errno));
		return 1;
	}

	lb_print_n(context->buffer);
	lb_free(context->buffer);
	return 0;
}
