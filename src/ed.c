#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "./lb.h"
#include "./ed.h"

// STRING UTILS

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

#define line_to_index(line) ((line) == 0 ? 0 : (line)-1)

typedef enum {
	ED_ERROR_NO_ERROR = 0,
	ED_ERROR_INVALID_COMMAND,
	ED_ERROR_INVALID_ADDRESS,
	ED_ERROR_INVALID_FILE,
	ED_ERROR_UNKNOWN,
} Ed_Error;

// CONTEXT

typedef struct {
	Line_Builder buffer;
	size_t line;
	char *filename;
	Line_Builder yank_register;
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

// Parse a address from user input.
//
// Returns `ED_ADDRESS_INVALID` upon failure.
// Sets `address` upon success, and `line` is updated to point to after the address.
// The return value specifies which member of the union for `address` has been set.
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

// Parse a command type from user input.
//
// Returns `ED_CMD_INVALID` upon failure.
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

bool address_out_of_range(Ed_Address address, Ed_Address_Type type,
			  bool allow_zero)
{
	Ed_Context *context = &ed_global_context;

	switch (type) {
	case ED_ADDRESS_START: {
		if (address.as_start == 0)
			return !allow_zero;
		size_t start = line_to_index(address.as_start);
		return !lb_contains(context->buffer, start);
	} break;
	case ED_ADDRESS_RANGE: {
		if (address.as_range.start == 0)
			return !allow_zero;
		size_t start = line_to_index(address.as_range.start);
		size_t end = line_to_index(address.as_range.end);
		if (end < start)
			return false;
		return !lb_contains(context->buffer, start) ||
		       !lb_contains(context->buffer, end);
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

	size_t start = line_to_index(address.as_start);
	return lb_contains(context->buffer, start);
}

bool ed_cmd_quit(bool *quit)
{
	// TODO: warning
	*quit = true;
	return true;
}

bool ed_cmd_num(Ed_Address address, Ed_Address_Type address_type)
{
	Ed_Context *context = &ed_global_context;

	if (address_out_of_range(address, address_type, false)) {
		context->error = ED_ERROR_INVALID_ADDRESS;
		return false;
	}

	if (address_type == ED_ADDRESS_START) {
		printf("%zu\n", address.as_start);
	} else {
		lb_num(context->buffer, address.as_range.start,
		       address.as_range.end);
	}

	return true;
}

bool ed_cmd_print(Ed_Address address, Ed_Address_Type address_type)
{
	Ed_Context *context = &ed_global_context;

	if (address_out_of_range(address, address_type, false)) {
		context->error = ED_ERROR_INVALID_ADDRESS;
		return false;
	}

	if (address_type == ED_ADDRESS_START) {
		size_t start = line_to_index(address.as_start);
		printf("%s", context->buffer.items[start]);
	} else {
		lb_print(context->buffer, address.as_range.start,
			 address.as_range.end);
	}

	return true;
}

bool ed_cmd_print_num(Ed_Address address, Ed_Address_Type address_type)
{
	Ed_Context *context = &ed_global_context;

	if (address_out_of_range(address, address_type, false)) {
		context->error = ED_ERROR_INVALID_ADDRESS;
		return false;
	}

	if (address_type == ED_ADDRESS_START) {
		size_t start = line_to_index(address.as_start);
		printf("%zu     %s", address.as_start,
		       context->buffer.items[start]);
	} else {
		lb_printn(context->buffer, address.as_range.start,
			  address.as_range.end);
	}

	return true;
}

bool ed_cmd_append(Ed_Address address, Ed_Address_Type address_type)
{
	Ed_Context *context = &ed_global_context;
	if (address_type != ED_ADDRESS_START ||
	    address_out_of_range(address, address_type, true)) {
		context->error = ED_ERROR_INVALID_ADDRESS;
		return false;
	}

	Line_Builder lb = { 0 };
	bool result = lb_read_to_dot(&lb);
	if (!result) {
		context->error = ED_ERROR_UNKNOWN;
		return false;
	}

	lb_insert(&context->buffer, &lb, address.as_start);
	return true;
}

bool ed_cmd_insert(Ed_Address address, Ed_Address_Type address_type)
{
	Ed_Context *context = &ed_global_context;
	if (address_type != ED_ADDRESS_START ||
	    address_out_of_range(address, address_type, true)) {
		context->error = ED_ERROR_INVALID_ADDRESS;
		return false;
	}

	Line_Builder lb = { 0 };
	bool result = lb_read_to_dot(&lb);
	if (!result) {
		context->error = ED_ERROR_UNKNOWN;
		return false;
	}

	context->line = address.as_start;
	lb_insert(&context->buffer, &lb, line_to_index(context->line));
	free(lb.items);

	return true;
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
		} else if (!lb_contains(context->buffer,
					line_to_index(address.as_start))) {
			context->error = ED_ERROR_INVALID_ADDRESS;
			return false;
		} else {
			context->line = address.as_start;
		}
	} break;
	case ED_CMD_QUIT: {
		return ed_cmd_quit(quit);
	} break;
	case ED_CMD_NUM: {
		return ed_cmd_num(address, address_type);
	} break;
	case ED_CMD_PRINT: {
		return ed_cmd_print(address, address_type);
	} break;
	case ED_CMD_PRINT_NUM: {
		return ed_cmd_print_num(address, address_type);
	} break;
	case ED_CMD_APPEND: {
		return ed_cmd_append(address, address_type);
	} break;
	case ED_CMD_INSERT: {
		return ed_cmd_insert(address, address_type);
	} break;
	case ED_CMD_CHANGE: {
		if (address_out_of_range(address, address_type, false)) {
			context->error = ED_ERROR_INVALID_ADDRESS;
			return false;
		}

		Line_Builder lb = { 0 };
		bool result = lb_read_to_dot(&lb);
		if (!result) {
			context->error = ED_ERROR_UNKNOWN;
			return false;
		}

		if (address_type == ED_ADDRESS_START) {
			size_t start = line_to_index(address.as_start);

			lb_append(&context->yank_register,
				  strdup(context->buffer.items[start]));
			lb_overwrite(&context->buffer, &lb, start, start);
			free(lb.items);
		} else {
			if (context->yank_register.items != NULL) {
				lb_clear(context->yank_register);
			}

			size_t index = line_to_index(address.as_range.start);
			lb_foreach(line, context->buffer)
			{
				index += 1;
				if (index < address.as_range.end) {
					lb_append(&context->yank_register,
						  strdup(*line));
					lb_pop_line(&context->buffer, index);
				}
			}
			lb_insert(&context->buffer, &lb,
				  address.as_range.start);
		}
	} break;
	case ED_CMD_DELETE: {
		if (address_out_of_range(address, address_type, false)) {
			context->error = ED_ERROR_INVALID_ADDRESS;
			return false;
		}

		if (address_type == ED_ADDRESS_START) {
			if (context->yank_register.items != NULL) {
				lb_clear(context->yank_register);
			}

			size_t start = line_to_index(address.as_start);

			lb_append(&context->yank_register,
				  strdup(context->buffer.items[start]));
			lb_pop_line(&context->buffer, start);
		} else {
			if (context->yank_register.items != NULL) {
				lb_clear(context->yank_register);
			}

			size_t start = line_to_index(address.as_range.start);
			size_t end = address.as_range.end;
			for (size_t i = start; i < end; ++i) {
				lb_append(&context->yank_register,
					  strdup(context->buffer.items[i]));
			}

			lb_pop_range(&context->buffer, start, end - 1);
		}
	} break;
	case ED_CMD_PUT: {
		if (address_out_of_range(address, address_type, true)) {
			context->error = ED_ERROR_INVALID_ADDRESS;
			return false;
		}

		Line_Builder tmp = { 0 };
		lb_foreach(line, context->yank_register)
		{
			lb_append(&tmp, strdup(*line));
		}

		lb_insert(&context->buffer, &tmp,
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

		bool result = lb_read_file(&context->buffer, f);
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

		lb_write_to_stream(context->buffer, f);
		fclose(f);
	} break;
	case ED_CMD_JOIN: {
		size_t start, end;
		if (address_type == ED_ADDRESS_START) {
			start = line_to_index(address.as_start);
			end = start + 1;
		} else {
			start = line_to_index(address.as_range.start);
			end = line_to_index(address.as_range.end);
		}

		if ((!lb_contains(context->buffer, start) && start != 0) ||
		    !lb_contains(context->buffer, end)) {
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

		lb_pop_range(&context->buffer, start + 1, end);
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
	lb_free(context->buffer);
	lb_free(context->yank_register);
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
