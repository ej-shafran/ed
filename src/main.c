#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

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

/*
 * `read_to_dot`
 *
 * Reads lines into `lb` from STDIN,
 * stopping when it reaches a line that is just one solitary ".".
 *
 * Returns `false` if `getline` fails;
 * `errno` is set to specify the cause.
 */
bool read_to_dot(Line_Builder *lb)
{
	while (true) {
		char *line = NULL;
		size_t nsize = 0;
		ssize_t nread = getline(&line, &nsize, stdin);
		if (nread < 0) {
			free(line);
			return false;
		}

		if (strcmp(line, ".\n") == 0)
			break;
		da_append(lb, line);
	}

	return true;
}

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
	Line_Builder target = { 0 };
	read_to_dot(&target);
	if (target.count < 2) {
		fprintf(stderr, "Please provide more than 2 lines.");
		lb_free(target);
		return 1;
	}

	Line_Builder source = { 0 };
	read_to_dot(&source);
	if (source.count < 1) {
		fprintf(stderr, "Please provide at least one line.");
		lb_free(target);
		lb_free(source);
		return 1;
	}

	insert_at(&target, &source, 2);

	lb_print(target);
	lb_free(target);

	return 0;
}
