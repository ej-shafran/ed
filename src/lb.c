#include "./lb.h"

bool lb_read_from_stream(Line_Builder *lb, FILE *file, char *condition)
{
	while (true) {
		char *line = NULL;
		size_t nsize = 0;
		ssize_t nread = getline(&line, &nsize, file);

		if (nread < 1) {
			if (line != NULL)
				free(line);
			return strlen(condition) == 0;
		}

		if (strcmp(line, condition) == 0) {
			free(line);
			return true;
		}

		lb_append(lb, line);
	}
}

void lb_insert(Line_Builder *target, Line_Builder *source, size_t index)
{
	assert(index <= target->count);

	realloc_chunk(target, source->count);

	memmove(target->items + index + source->count,
		target->items + index,
		(target->count - index) * sizeof(*target->items));
	target->count += source->count;

	memcpy(target->items + index, source->items,
	       source->count * sizeof(source->items));
}

void lb_overwrite(Line_Builder *target, Line_Builder *source, size_t start,
		  size_t end)
{
	assert(end < target->count);

	size_t amount = end + 1 - start;

	realloc_chunk(target, source->count + amount);

	for (size_t i = start; i <= end; ++i)
		free(target->items[i]);

	memmove(target->items + start + source->count, target->items + end + 1,
		(target->count - end - 1) * sizeof(*target->items));
	target->count += source->count - amount;

	memcpy(target->items + start, source->items,
	       source->count * sizeof(*source->items)); // maybe no *?
}

void lb_pop_line(Line_Builder *target, size_t index)
{
	assert(index < target->count);

	free(target->items[index]);

	memmove(target->items + index, target->items + index + 1,
		(target->count - index - 1) * sizeof(*target->items));
	target->count -= 1;
}

void lb_pop_range(Line_Builder *target, size_t start, size_t end)
{
	assert(end < target->count);

	for (size_t i = start; i <= end; ++i)
		free(target->items[i]);

	memmove(target->items + start, target->items + end + 1,
		(target->count - end - 1) * sizeof(*target->items));
	target->count -= end - start + 1;
}

bool lb_contains(Line_Builder lb, size_t n)
{
    return n < lb.count;
}
