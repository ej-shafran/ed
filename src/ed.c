#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "ed.h"

typedef enum {
	ED_ERROR_NO_ERROR = 0,
	ED_ERROR_INVALID_COMMAND,
	ED_ERROR_INVALID_ADDRESS,
	ED_ERROR_INVALID_FILE,
	ED_ERROR_UNKNOWN,
} Ed_Error;

// Globally-shared context within the application logic.
typedef struct {
	Ed_Line_Builder buffer;
	size_t line;
	char *filename;
	Ed_Line_Builder yank_register;
	Ed_Error error;
	bool prompt;
	bool should_print_error;
} Ed_Context;

Ed_Context ed_global_context = { .buffer = { 0 },
				 .line = 0,
				 .yank_register = { 0 },
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

		if (nread < 1) {
			if (line != NULL)
				free(line);
			return strlen(condition) == 0;
		}

		if (strcmp(line, condition) == 0) {
			free(line);
			return true;
		}

		da_append(lb, line);
	}
}

void ed_lb_insert_at(Ed_Line_Builder *target, Ed_Line_Builder *source,
		     size_t address)
{
	assert(address <= target->count);

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

	memmove(target->items + address + source->count,
		target->items + address,
		(target->count - address) * sizeof(*target->items));
	target->count += source->count;

	memcpy(target->items + address, source->items,
	       source->count * sizeof(source->items));
}

void ed_lb_pop_and_insert(Ed_Line_Builder *target, Ed_Line_Builder *source,
			  size_t address)
{
	assert(address < target->count);

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

	memmove(target->items + address + source->count,
		target->items + address + 1,
		(target->count - address - 1) * sizeof(*target->items));
	target->count += source->count;

	free(target->items[address]);
	memcpy(target->items + address, source->items,
	       source->count * sizeof(source->items));
}

void ed_lb_pop(Ed_Line_Builder *target, size_t address)
{
	assert(address < target->count);

	free(target->items[address]);

	memmove(target->items + address, target->items + address + 1,
		(target->count - address - 1) * sizeof(*target->items));
	target->count -= 1;
}

Ed_Address_Type ed_parse_address(char **line, Ed_Address *address)
{
	Ed_Context *context = &ed_global_context;

	Ed_Address_Type result;

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
			address->as_range.start = 1;
			address->as_range.end = last_line;
			c += 1;
			result = ED_ADDRESS_RANGE;
			goto defer;
		} else {
			address->as_start = context->line;
			result = ED_ADDRESS_START;
			goto defer;
		}
	}

	if (*c != ',') {
		address->as_start = start;
		result = ED_ADDRESS_START;
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
			result = ED_ADDRESS_INVALID;
			goto defer;
		}
	}

	if (start > end) {
		result = ED_ADDRESS_INVALID;
		goto defer;
	}

	if (start == end) {
		address->as_start = start;
		result = ED_ADDRESS_START;
		goto defer;
	}

	address->as_range.start = start;
	address->as_range.end = end;
	result = ED_ADDRESS_RANGE;
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
	case 'd':
		return ED_CMD_DELETE;
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
	case 'n':
		return ED_CMD_NUM;
	case 'p':
		*line += 1;
		if (*line[0] == 'n')
			return ED_CMD_PRINT_NUM;
		return ED_CMD_PRINT;
	case 'P':
		return ED_CMD_TOGGLE_PROMPT;
	case 'q':
		return ED_CMD_QUIT;
	case 'w':
		*line += 1;
		*line = trim(*line);
		return ED_CMD_WRITE;
	case 'x':
		return ED_CMD_PUT;
	default:
		return ED_CMD_INVALID;
	}
}

bool out_of_buffer(Ed_Line_Builder buffer, size_t n)
{
	return n >= buffer.count || (n < 1 && !(n == 0 && buffer.count == 0));
}

bool address_out_of_range(Ed_Address address, Ed_Address_Type type)
{
	Ed_Context *context = &ed_global_context;

	switch (type) {
	case ED_ADDRESS_START: {
		return out_of_buffer(context->buffer, address.as_start);
	} break;
	case ED_ADDRESS_RANGE: {
		return out_of_buffer(context->buffer, address.as_range.start) ||
		       out_of_buffer(context->buffer, address.as_range.end);
	} break;
	case ED_ADDRESS_INVALID: {
		return false;
	} break;
	default: {
		assert(0 && "Unreachable");
		exit(1);
	} break;
	}
}

bool ensure_address_start(Ed_Address address, Ed_Address_Type address_type)
{
	Ed_Context *context = &ed_global_context;

	if (address_type != ED_ADDRESS_START)
		return false;

	return !out_of_buffer(context->buffer, address.as_start);
}

bool ed_handle_cmd(char *line, bool *quit)
{
	Ed_Context *context = &ed_global_context;

	Ed_Address address = { 0 };
	Ed_Address_Type address_type = ed_parse_address(&line, &address);

	Ed_Cmd_Type cmd_type = ed_parse_cmd_type(&line);
	switch (cmd_type) {
	case ED_CMD_INVALID: {
		if (address_type != ED_ADDRESS_START) {
			context->error = ED_ERROR_INVALID_COMMAND;
			return false;
		} else if (out_of_buffer(context->buffer, address.as_start)) {
			context->error = ED_ERROR_INVALID_ADDRESS;
			return false;
		} else {
			context->line = address.as_start;
		}
	} break;
	case ED_CMD_QUIT: {
		// TODO: warning
		*quit = true;
		return true;
	} break;
	case ED_CMD_NUM: {
		if (address_out_of_range(address, address_type)) {
			context->error = ED_ERROR_INVALID_ADDRESS;
			return false;
		}

		if (address_type == ED_ADDRESS_START) {
			printf("%zu\n", address.as_start);
		} else {
			ed_lb_num(context->buffer, address.as_range.start,
				  address.as_range.end);
		}
	} break;
	case ED_CMD_PRINT: {
		if (address_out_of_range(address, address_type)) {
			context->error = ED_ERROR_INVALID_ADDRESS;
			return false;
		}

		if (address_type == ED_ADDRESS_START) {
			printf("%s",
			       context->buffer.items[address.as_start - 1]);
		} else {
			ed_lb_print(context->buffer, address.as_range.start,
				    address.as_range.end);
		}
	} break;
	case ED_CMD_PRINT_NUM: {
		if (address_out_of_range(address, address_type)) {
			context->error = ED_ERROR_INVALID_ADDRESS;
			return false;
		}

		if (address_type == ED_ADDRESS_START) {
			printf("%zu     %s", address.as_start,
			       context->buffer.items[address.as_start - 1]);
		} else {
			ed_lb_printn(context->buffer, address.as_range.start,
				     address.as_range.end);
		}
	} break;
	case ED_CMD_APPEND: {
		if (!ensure_address_start(address, address_type)) {
			context->error = ED_ERROR_INVALID_ADDRESS;
			return false;
		}

		Ed_Line_Builder lb = { 0 };
		bool result = ed_lb_read_to_dot(&lb);
		if (!result) {
			context->error = ED_ERROR_UNKNOWN;
			return false;
		}

		ed_lb_insert_at(&context->buffer, &lb, address.as_start);
	} break;
	case ED_CMD_INSERT: {
		if (!ensure_address_start(address, address_type)) {
			context->error = ED_ERROR_INVALID_ADDRESS;
			return false;
		}

		Ed_Line_Builder lb = { 0 };
		bool result = ed_lb_read_to_dot(&lb);
		if (!result) {
			context->error = ED_ERROR_UNKNOWN;
			return false;
		}

		ed_lb_insert_at(&context->buffer, &lb, address.as_start - 1);
	} break;
	case ED_CMD_CHANGE: {
		if (!ensure_address_start(address, address_type)) {
			context->error = ED_ERROR_INVALID_ADDRESS;
			return false;
		}

		Ed_Line_Builder lb = { 0 };
		bool result = ed_lb_read_to_dot(&lb);
		if (!result) {
			context->error = ED_ERROR_UNKNOWN;
			return false;
		}

		da_append(&context->yank_register,
			  strdup(context->buffer.items[address.as_start - 1]));
		ed_lb_pop_and_insert(&context->buffer, &lb,
				     address.as_start - 1);
		free(lb.items);
	} break;
	case ED_CMD_DELETE: {
		if (address_out_of_range(address, address_type)) {
			context->error = ED_ERROR_INVALID_ADDRESS;
			return false;
		}

		if (address_type == ED_ADDRESS_START) {
			if (context->yank_register.items != NULL) {
				ed_lb_clear(context->yank_register);
			}

			da_append(&context->yank_register,
				  strdup(context->buffer
						 .items[address.as_start - 1]));
			ed_lb_pop(&context->buffer, address.as_start - 1);
		} else {
			if (context->yank_register.items != NULL) {
				ed_lb_clear(context->yank_register);
			}
			size_t index = address.as_range.start - 1;
			da_foreach(line, context->buffer)
			{
				index += 1;
				if (index < address.as_range.end) {
					da_append(&context->yank_register,
						  strdup(*line));
					ed_lb_pop(&context->buffer, index);
				}
			}
		}
	} break;
	case ED_CMD_PUT: {
		if (address_out_of_range(address, address_type)) {
			context->error = ED_ERROR_INVALID_ADDRESS;
			return false;
		}

		Ed_Line_Builder tmp = { 0 };
		da_foreach(line, context->yank_register)
		{
			da_append(&tmp, strdup(*line));
		}

		ed_lb_insert_at(&context->buffer, &tmp,
				address_type == ED_ADDRESS_START ?
					address.as_start - 1 :
					address.as_range.end - 1);
		free(tmp.items);
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

	free(context->filename);
	ed_lb_free(context->buffer);
	ed_lb_free(context->yank_register);
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
	case ED_ERROR_INVALID_ADDRESS: {
		fprintf(stderr, "Invalid address.\n");
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
