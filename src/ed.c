#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "ed.h"

// LINE BUILDER

// Dynamic array of lines (nul-terminated with the '\n' at the end)
typedef da(char *) Ed_Line_Builder;

// Read lines from `stream` into `lb` until `condition` is met.
// Passing `""` as the condition will read until EOF.
bool ed_lb_read_from_stream(Ed_Line_Builder *lb, FILE *file, char *condition);

// Write all lines from `lb` into `stream`.
#define ed_lb_write_to_stream(lb, file)          \
	da_foreach(line, lb)                     \
	{                                        \
		int result = fputs(*line, file); \
		if (result < 0)                  \
			break;                   \
	}

// Insert the contents of `source` into `target` at index `address`,
// pushing off the contents of `target` to make room.
void ed_lb_insert_at(Ed_Line_Builder *target, Ed_Line_Builder *source,
		     size_t address);

// Insert the contents of `source` into `target` at index `address`,
// while removing the line at `address` from the target.
void ed_lb_pop_and_insert(Ed_Line_Builder *target, Ed_Line_Builder *source,
			  size_t address);

// Remove the line at `address` from `target`.
void ed_lb_pop(Ed_Line_Builder *target, size_t address);

// Wrapper around `ed_lb_read_from_stream` that reads lines from STDIN
// until a line with just `"."` is encountered.
#define ed_lb_read_to_dot(lb) ed_lb_read_from_stream(lb, stdin, ".\n")

// Wrapper around `ed_lb_read_from_stream` that reads lines from `file` until EOF.
#define ed_lb_read_file(lb, file) ed_lb_read_from_stream(lb, file, "")

// Print lines from `lb` into `stream`
#define ed_lb_fprint(stream, lb, start, end)                  \
	do {                                                  \
		size_t i = 0;                                 \
		da_foreach(line, lb)                          \
		{                                             \
			i += 1;                               \
			if (i >= start && i <= end)           \
				fprintf(stream, "%s", *line); \
		}                                             \
	} while (0);

// Wrapper around `ed_lb_fprint` which prints to STDOUT.
#define ed_lb_print(lb, start, end) ed_lb_fprint(stdout, lb, start, end)

// Print line numbers from `lb` into `stream`
#define ed_lb_fnum(stream, lb, start, end)                   \
	do {                                                 \
		size_t i = 0;                                \
		da_foreach(line, lb)                         \
		{                                            \
			i += 1;                              \
			if (i >= start && i <= end)          \
				fprintf(stream, "%zu\n", i); \
		}                                            \
	} while (0);

// Wrapper around `ed_lb_fnum` which prints to STDOUT.
#define ed_lb_num(lb, start, end) ed_lb_fnum(stdout, lb, start, end)

// Print lines and their line numbers from `lb` into `stream`
#define ed_lb_fprintn(stream, lb, start, end)                            \
	do {                                                             \
		size_t i = 0;                                            \
		da_foreach(line, lb)                                     \
		{                                                        \
			i += 1;                                          \
			if (i >= start && i <= end)                      \
				fprintf(stream, "%zu     %s", i, *line); \
		}                                                        \
	} while (0);

// Wrapper around `ed_lb_fprintn` which prints to STDOUT.
#define ed_lb_printn(lb, start, end) ed_lb_fprintn(stdout, lb, start, end)

// Free the allocated pointers in `lb`.
#define ed_lb_free(lb)                  \
	do {                            \
		da_foreach(line, lb)    \
		{                       \
			free(*line);    \
		}                       \
		if (lb.items != NULL)   \
			free(lb.items); \
	} while (0);

#define ed_lb_clear(lb)          \
	do {                     \
		ed_lb_free(lb);  \
		lb.count = 0;    \
		lb.items = NULL; \
		lb.capacity = 0; \
	} while (0);

// COMMANDS

typedef enum {
	ED_CMD_APPEND = 0,
	ED_CMD_INSERT,
	ED_CMD_CHANGE,
	ED_CMD_DELETE,
	ED_CMD_PUT,
	ED_CMD_EDIT,
	ED_CMD_WRITE,
	ED_CMD_NUM,
	ED_CMD_PRINT,
	ED_CMD_PRINT_NUM,
	ED_CMD_QUIT,
	ED_CMD_LAST_ERR,
	ED_CMD_TOGGLE_PROMPT,
	ED_CMD_TOGGLE_ERR,
	ED_CMD_INVALID,
	ED_CMD_JOIN,
} Ed_Cmd_Type;

typedef struct {
	size_t start;
	size_t end;
} Ed_Range;

typedef enum {
	ED_ADDRESS_START = 0,
	ED_ADDRESS_RANGE,
	ED_ADDRESS_INVALID
} Ed_Address_Type;

typedef union {
	size_t as_start;
	Ed_Range as_range;
} Ed_Address;

// Parse a address from user input.
//
// Returns `ED_ADDRESS_INVALID` upon failure.
// Sets `address` upon success, and `line` is updated to point to after the address.
// The return value specifies which member of the union for `address` has been set.
Ed_Address_Type ed_parse_address(char **line, Ed_Address *address);

// Parse a command type from user input.
//
// Returns `ED_CMD_INVALID` upon failure.
Ed_Cmd_Type ed_parse_cmd_type(char **line);

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

void ed_lb_pop_range(Ed_Line_Builder *target, size_t start, size_t end)
{
	assert(end < target->count);

	for (size_t i = start; i <= end; ++i)
		free(target->items[i]);

	memmove(target->items + start, target->items + end + 1,
		(target->count - end - 1) * sizeof(*target->items));
	target->count -= end - start + 1;
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
		if (*c == '.') {
			start = context->line;
			c += 1;
		} else if (*c == '$') {
			start = context->buffer.count;
			c += 1;
		} else if (*c == ',') {
			address->as_range.start = 1;
			address->as_range.end = context->buffer.count;
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
	case 'j':
		return ED_CMD_JOIN;
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
	if (n == 0 && buffer.count == 0)
		return true;
	return n > buffer.count || n < 1;
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

char *strappend(char *a, char *b)
{
	int sizea = strlen(a);
	int sizeb = strlen(b);
	int size = sizea + sizeb + 1;

	char *s = calloc(size, sizeof(char));

	for (int i = 0; i < sizea; ++i)
		s[i] = a[i];

	for (int i = 0; i <= sizeb; ++i)
		s[sizea + i] = b[i];

	return s;
}

bool ensure_address_start(Ed_Address address, Ed_Address_Type address_type)
{
	Ed_Context *context = &ed_global_context;

	if (address_type != ED_ADDRESS_START)
		return false;

	return !out_of_buffer(context->buffer, address.as_start);
}

// API
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
		if (!ensure_address_start(address, address_type) &&
		    address.as_start != 0) {
			context->error = ED_ERROR_INVALID_ADDRESS;
			return false;
		}

		Ed_Line_Builder lb = { 0 };
		bool result = ed_lb_read_to_dot(&lb);
		if (!result) {
			context->error = ED_ERROR_UNKNOWN;
			return false;
		}

		size_t line = address.as_start > 0 ? address.as_start - 1 : 0;
		context->line = line + lb.count;
		ed_lb_insert_at(&context->buffer, &lb, line);
		free(lb.items);
	} break;
	case ED_CMD_CHANGE: {
		if (address_out_of_range(address, address_type)) {
			context->error = ED_ERROR_INVALID_ADDRESS;
			return false;
		}

		Ed_Line_Builder lb = { 0 };
		bool result = ed_lb_read_to_dot(&lb);
		if (!result) {
			context->error = ED_ERROR_UNKNOWN;
			return false;
		}

		if (address_type == ED_ADDRESS_START) {
			da_append(&context->yank_register,
				  strdup(context->buffer
						 .items[address.as_start - 1]));
			ed_lb_pop_and_insert(&context->buffer, &lb,
					     address.as_start - 1);
			free(lb.items);
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
			ed_lb_insert_at(&context->buffer, &lb,
					address.as_range.start);
		}
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

			size_t start = address.as_range.start;
			size_t end = address.as_range.end;
			for (size_t i = start - 1; i < end; ++i) {
				da_append(&context->yank_register,
					  strdup(context->buffer.items[i]));
			}

			ed_lb_pop_range(&context->buffer,
					start > 0 ? start - 1 : 0, end - 1);
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
					address.as_start :
					address.as_range.end);
		free(tmp.items);
	} break;
	case ED_CMD_EDIT: {
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
			free(context->filename);
			context->filename = strdup(line);
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
	case ED_CMD_JOIN: {
		size_t start, end;
		if (address_type == ED_ADDRESS_START) {
			start = address.as_start - 1;
			end = start + 1;
		} else {
			start = address.as_range.start - 1;
			end = address.as_range.end - 1;
		}

		if ((out_of_buffer(context->buffer, start) && start != 0) ||
		    out_of_buffer(context->buffer, end)) {
			context->error = ED_ERROR_INVALID_ADDRESS;
			return false;
		}

		for (size_t i = start + 1; i <= end; ++i) {
			int len = strlen(context->buffer.items[start]);
			context->buffer.items[start][len - 1] = '\0';

			char *result = strappend(context->buffer.items[start],
						 context->buffer.items[i]);
			free(context->buffer.items[start]);
			context->buffer.items[start] = result;
		}

		ed_lb_pop_range(&context->buffer, start + 1, end);
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
