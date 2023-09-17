#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "ed.h"

bool ed_lb_read_from_stream(Ed_Line_Builder *lb, FILE *file, char *condition)
{
	while (true) {
		char *line = NULL;
		size_t nsize = 0;
		ssize_t nread = getline(&line, &nsize, file);

		if (strcmp(line, condition) == 0)
			return true;

		if (nread < 1) {
			free(line);
			return false;
		}

		da_append(lb, line);
	}
}

void ed_lb_insert_at(Ed_Line_Builder *target, Ed_Line_Builder *source,
		     size_t location)
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


void ed_lb_pop_and_insert(Ed_Line_Builder *target, Ed_Line_Builder *source, size_t location)
{
    assert(location < target->count);

	if (target->count + source->count - 1 > target->capacity) {
		if (target->capacity == 0) {
			target->capacity = DA_CAP_INIT;
		}
		while (target->count + source->count - 1 > target->capacity) {
			target->capacity *= 2;
		}
		target->items =
			realloc(target->items,
				target->capacity * sizeof(*target->items));
		assert(target->items != NULL && "Could not reallocate memory");
	}

    memmove(target->items + location + source->count,
            target->items + location + 1,
            (target->count - location - 1) * sizeof(*target->items));
	target->count += source->count;

	memcpy(target->items + location, source->items,
	       source->count * sizeof(source->items));

	source->items = NULL;
	source->count = 0;
	source->capacity = 0;
}

Ed_Location_Type ed_parse_location(char **line, Ed_Location *location)
{
	char *c = *line;
	bool any = false;

	size_t start = 0;
	for (; isdigit(*c); c++) {
		any = true;
		start *= 10;
		start += *c - '0';
	}

	if (!any)
		return ED_LOCATION_INVALID;

	if (*c != ',') {
		*line = c;
		location->as_start = start;
		return ED_LOCATION_START;
	}

	// remove ','
	c += 1;
	any = false;

	size_t end = 0;
	for (; isdigit(*c); c++) {
		any = true;
		end *= 10;
		end += *c - '0';
	}

	if (!any)
		return ED_LOCATION_INVALID;

	*line = c;
	location->as_range.start = start;
	location->as_range.end = end;
	return ED_LOCATION_RANGE;
}
