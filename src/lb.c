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

void lb_pop(Line_Builder *target, size_t start, size_t end)
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

void lb_clone(Line_Builder *a, Line_Builder *b)
{
	lb_clear(*b);
	lb_foreach(line, *a)
	{
		lb_append(b, strdup(*line));
	}
}

void lb_swap(Line_Builder *a, Line_Builder *b)
{
	char **temp_items = b->items;
	size_t temp_count = b->count;
	size_t temp_cap = b->capacity;

	b->items = a->items;
	b->count = a->count;
	b->capacity = a->capacity;

	a->items = temp_items;
	a->count = temp_count;
	a->capacity = temp_cap;
}
