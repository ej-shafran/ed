#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "ed.h"

typedef enum {
	ED_ERROR_NO_ERROR = 0,
	ED_ERROR_INVALID_COMMAND,
	ED_ERROR_INVALID_LOCATION,
	ED_ERROR_INVALID_FILE,
	ED_ERROR_UNKNOWN,
} Ed_Error;

// Globally-shared context within the application logic.
typedef struct {
	Ed_Line_Builder buffer;
	size_t line;
	char *filename;
	Ed_Error error;
	bool prompt;
	bool should_print_error;
} Ed_Context;

Ed_Context ed_global_context = { .buffer = { 0 },
				 .line = 0,
				 .filename = 0,
				 .error = ED_ERROR_NO_ERROR,
				 .prompt = false,
				 .should_print_error = false };

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

void ed_lb_pop_and_insert(Ed_Line_Builder *target, Ed_Line_Builder *source,
			  size_t location)
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
	Ed_Context *context = &ed_global_context;

	Ed_Location_Type result;

	char *c = *line;
	bool any = false;

	size_t start = 0;
	for (; isdigit(*c); c++) {
		any = true;
		start *= 10;
		start += *c - '0';
	}

	if (!any) {
		size_t last_line = context->buffer.count == 0 ?
					   0 :
					   context->buffer.count - 1;
		if (*c == '.') {
			start = context->line;
			c += 1;
		} else if (*c == '$') {
			start = last_line;
			c += 1;
		} else if (*c == ',') {
			location->as_range.start = 1;
			location->as_range.end = last_line;
			c += 1;
			result = ED_LOCATION_RANGE;
			goto defer;
		} else {
			location->as_start = context->line;
			result = ED_LOCATION_START;
			goto defer;
		}
	}

	if (*c != ',') {
		location->as_start = start;
		result = ED_LOCATION_START;
		goto defer;
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

	if (!any) {
		if (*c == '.') {
			end = context->line;
			c += 1;
		} else if (*c == '$') {
			end = context->buffer.count - 1;
			c += 1;
		} else {
			result = ED_LOCATION_INVALID;
			goto defer;
		}
	}

	if (start > end) {
		result = ED_LOCATION_INVALID;
		goto defer;
	}

	if (start == end) {
		location->as_start = start;
		result = ED_LOCATION_START;
		goto defer;
	}

	location->as_range.start = start;
	location->as_range.end = end;
	result = ED_LOCATION_RANGE;
	goto defer;

defer:
	*line = c;
	return result;
}

char *ltrim(char *s)
{
	while (isspace(*s))
		s++;
	return s;
}

char *rtrim(char *s)
{
	char *back = s + strlen(s);
	while (isspace(*--back))
		;
	*(back + 1) = '\0';
	return s;
}

char *trim(char *s)
{
	return rtrim(ltrim(s));
}

Ed_Cmd_Type ed_parse_cmd_type(char **line)
{
	switch (*line[0]) {
	case 'a':
		return ED_CMD_APPEND;
	case 'c':
		return ED_CMD_CHANGE;
	case 'e':
		*line += 1;
		*line = trim(*line);
		return ED_CMD_EDIT;
	case 'h':
		return ED_CMD_LAST_ERR;
	case 'H':
		return ED_CMD_TOGGLE_ERR;
	case 'i':
		return ED_CMD_INSERT;
	case 'p':
		return ED_CMD_PRINT;
	case 'P':
		return ED_CMD_TOGGLE_PROMPT;
	case 'q':
		return ED_CMD_QUIT;
	case 'w':
		*line += 1;
		*line = trim(*line);
		return ED_CMD_WRITE;
	default:
		return ED_CMD_INVALID;
	}
}

bool out_of_buffer(Ed_Line_Builder buffer, size_t n)
{
	return n >= buffer.count || (n < 1 && !(n == 0 && buffer.count == 0));
}

bool ensure_location_start(Ed_Location location, Ed_Location_Type loc_type)
{
	Ed_Context *context = &ed_global_context;

	if (loc_type != ED_LOCATION_START)
		return false;

	return !out_of_buffer(context->buffer, location.as_start);
}

bool ed_handle_cmd(char *line, bool *quit)
{
	Ed_Context *context = &ed_global_context;

	Ed_Location location = { 0 };
	Ed_Location_Type loc_type = ed_parse_location(&line, &location);

	Ed_Cmd_Type cmd_type = ed_parse_cmd_type(&line);
	switch (cmd_type) {
	case ED_CMD_INVALID: {
		if (loc_type != ED_LOCATION_START) {
			context->error = ED_ERROR_INVALID_COMMAND;
			return false;
		} else if (out_of_buffer(context->buffer, location.as_start)) {
			context->error = ED_ERROR_INVALID_LOCATION;
			return false;
		} else {
			context->line = location.as_start;
		}
	} break;
	case ED_CMD_QUIT: {
		*quit = true;
		return true;
	} break;
	case ED_CMD_PRINT: {
		if (loc_type == ED_LOCATION_START) {
			if (out_of_buffer(context->buffer, location.as_start)) {
				context->error = ED_ERROR_INVALID_LOCATION;
				return false;
			}
			printf("%s", context->buffer.items[location.as_start]);
		} else {
			if (out_of_buffer(context->buffer,
					  location.as_range.start) ||
			    out_of_buffer(context->buffer,
					  location.as_range.end)) {
				context->error = ED_ERROR_INVALID_LOCATION;
				return false;
			}
			ed_lb_print(context->buffer, location.as_range.start,
				    location.as_range.end);
		}
	} break;
	case ED_CMD_APPEND: {
		if (!ensure_location_start(location, loc_type)) {
			context->error = ED_ERROR_INVALID_LOCATION;
			return false;
		}

		Ed_Line_Builder lb = { 0 };
		bool result = ed_lb_read_to_dot(&lb);
		if (!result) {
			context->error = ED_ERROR_UNKNOWN;
			return false;
		}

		ed_lb_insert_at(&context->buffer, &lb, location.as_start + 1);
	} break;
	case ED_CMD_INSERT: {
		if (!ensure_location_start(location, loc_type)) {
			context->error = ED_ERROR_INVALID_LOCATION;
			return false;
		}

		Ed_Line_Builder lb = { 0 };
		bool result = ed_lb_read_to_dot(&lb);
		if (!result) {
			context->error = ED_ERROR_UNKNOWN;
			return false;
		}

		ed_lb_insert_at(&context->buffer, &lb, location.as_start);
	} break;
	case ED_CMD_CHANGE: {
		if (!ensure_location_start(location, loc_type)) {
			context->error = ED_ERROR_INVALID_LOCATION;
			return false;
		}

		Ed_Line_Builder lb = { 0 };
		bool result = ed_lb_read_to_dot(&lb);
		if (!result) {
			context->error = ED_ERROR_UNKNOWN;
			return false;
		}

		ed_lb_pop_and_insert(&context->buffer, &lb, location.as_start);
	} break;
	case ED_CMD_EDIT: {
		// TODO: do we need strdup here?
		context->filename = strdup(line);

		FILE *f = fopen(line, "r");
		if (f == NULL) {
			context->error = ED_ERROR_INVALID_FILE;
			return false;
		}

		bool result = ed_lb_read_file(&context->buffer, f);
		context->line = context->buffer.count > 0 ?
					context->buffer.count - 1 :
					0;
		fclose(f);
		if (!result)
			context->error = ED_ERROR_UNKNOWN;
		return result;
	} break;
	case ED_CMD_WRITE: {
		if (strlen(line) != 0) {
			ed_lb_clear(context->buffer);
			context->filename = line;
		}

		if (context->filename == NULL ||
		    strlen(context->filename) == 0) {
			context->error = ED_ERROR_INVALID_COMMAND;
			return false;
		}

		FILE *f = fopen(context->filename, "w");
		if (f == NULL) {
			context->error = ED_ERROR_INVALID_FILE;
			return false;
		}

		ed_lb_write_to_stream(context->buffer, f);
		fclose(f);
	} break;
	case ED_CMD_LAST_ERR: {
		ed_print_error();
		return true;
	} break;
	case ED_CMD_TOGGLE_PROMPT: {
		context->prompt = !context->prompt;
		return true;
	} break;
	case ED_CMD_TOGGLE_ERR: {
		context->should_print_error = !context->should_print_error;
		return true;
	} break;
	}

	return true;
}

void ed_cleanup()
{
	Ed_Context *context = &ed_global_context;

	ed_lb_free(context->buffer);
}

bool ed_should_print_error()
{
	Ed_Context *context = &ed_global_context;

	return context->should_print_error;
}

void ed_print_error()
{
	Ed_Context *context = &ed_global_context;

	switch (context->error) {
	case ED_ERROR_NO_ERROR: {
		fprintf(stderr, "No error.\n");
	} break;
	case ED_ERROR_INVALID_COMMAND: {
		fprintf(stderr, "Invalid command.\n");
	} break;
	case ED_ERROR_INVALID_FILE: {
		fprintf(stderr, "Could not open file.\n");
	} break;
	case ED_ERROR_INVALID_LOCATION: {
		fprintf(stderr, "Invalid location.\n");
	} break;
	default: {
		fprintf(stderr, "Unknown error.\n");
	} break;
	}
}

ssize_t ed_getline(char **lineptr, size_t *n, FILE *stream)
{
	Ed_Context *context = &ed_global_context;

	if (context->prompt)
		printf("*");

	return getline(lineptr, n, stream);
}
