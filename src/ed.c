#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "./lb.h"
#include "./ed.h"

// STRING UTILS

// Trim a string of whitespace, from both ends.
char *trim(char *s)
{
	// trim off left end
	while (isspace(*s))
		++s;

	// pointer to the end of the string (excluding '\0')
	char *back = s + strlen(s) - 1;
	// trim off right end
	while (isspace(*back))
		--back;
	*(back + 1) = '\0';

	return s;
}

// Dynamically concatenate two strings, returning the result.
//
// The returned string is allocated with `calloc` and needs to be freed.
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

// Enumeration of all possible `ed` commands.
typedef enum {
	ED_CMD_APPEND = 0,
	ED_CMD_CHANGE,
	ED_CMD_DELETE,
	ED_CMD_EDIT,
	ED_CMD_FORCE_QUIT,
	ED_CMD_INSERT,
	ED_CMD_JOIN,
	ED_CMD_LAST_ERR,
	ED_CMD_MOVE,
	ED_CMD_PRINT,
	ED_CMD_PRINT_NUM,
	ED_CMD_PUT,
	ED_CMD_QUIT,
	ED_CMD_TOGGLE_ERR,
	ED_CMD_TOGGLE_PROMPT,
	ED_CMD_UNDO,
	ED_CMD_WRITE,
	ED_CMD_INVALID,
} Ed_Cmd_Type;

// ADDRESSES

// A range of lines written as `start,end` before a command.
typedef struct {
	size_t start;
	size_t end;
} Ed_Range;

// An enumeration of the possible states the `Ed_Address_Position` union can be in.
typedef enum {
	ED_ADDRESS_LINE = 0,
	ED_ADDRESS_RANGE,
	ED_ADDRESS_INVALID
} Ed_Address_Type;

// Either a line, specified as `number` before the command, or a range.
typedef union {
	size_t as_line;
	Ed_Range as_range;
} Ed_Address_Position;

// The address over which a command takes place.
typedef struct {
	Ed_Address_Position position;
	Ed_Address_Type type;
} Ed_Address;

// Convert a line number to an index within the buffer.
#define line_to_index(line) ((line) == 0 ? 0 : (line)-1)

// ERRORS

// Enumeration over the possible errors which may occur.
typedef enum {
	ED_ERROR_NO_ERROR = 0,
	ED_ERROR_INVALID_ADDRESS,
	ED_ERROR_INVALID_COMMAND,
	ED_ERROR_INVALID_FILE,
	ED_ERROR_NO_UNDO,
	ED_ERROR_UNSAVED_CHANGES,
	ED_ERROR_UNKNOWN,
} Ed_Error;

// CONTEXT

// Struct with all of the global context for the application.
typedef struct {
	Line_Builder buffer;
	size_t change_count;
	Line_Builder back_buf;
	size_t back_changes;

	size_t line;
	char *filename;
	Line_Builder yank_register;
	Ed_Error error;
	bool prompt;
	bool should_print_error;
} Ed_Context;

// Instance of `Ed_Context` that is shared globally.
static Ed_Context ed_global_context = { .buffer = { 0 },
					.change_count = 0,
					.back_buf = { 0 },
					.back_changes = 0,

					.line = 0,
					.yank_register = { 0 },
					.filename = 0,
					.error = ED_ERROR_NO_ERROR,
					.prompt = false,
					.should_print_error = false };

// Like `lb_pop` for the global context's buffer.
void ed_context_pop(size_t start, size_t end)
{
	Ed_Context *context = &ed_global_context;
	lb_clone(&context->buffer, &context->back_buf);
	context->back_changes = context->change_count;
	lb_pop(&context->buffer, start, end);
	context->change_count += 1;
}

// Like `lb_insert` for the global context's buffer.
void ed_context_insert(Line_Builder *lb, size_t index)
{
	Ed_Context *context = &ed_global_context;
	lb_clone(&context->buffer, &context->back_buf);
	context->back_changes = context->change_count;
	lb_insert(&context->buffer, lb, index);
	context->change_count += 1;
}

// Like `lb_overwrite` for the global context's buffer.
void ed_context_overwrite(Line_Builder *lb, size_t start, size_t end)
{
	Ed_Context *context = &ed_global_context;
	lb_clone(&context->buffer, &context->back_buf);
	context->back_changes = context->change_count;
	lb_overwrite(&context->buffer, lb, start, end);
	context->change_count += 1;
}

// Sets the global context's error.
void ed_context_set_error(Ed_Error error)
{
	Ed_Context *context = &ed_global_context;
	context->error = error;
}

// Sets the global context's error and returns `false`.
#define ed_return_error(error)               \
	do {                                 \
		ed_context_set_error(error); \
		return false;                \
	} while (0);

// PARSING

// Parse an address from user input.
//
// Returns an address with type `ED_ADDRESS_INVALID` upon failure.
// `line` is updated to point to after the address specifier.
// TODO: rewrite this to be more readable and to handle other cases
Ed_Address ed_parse_address(char **line)
{
	Ed_Address address = { 0 };
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
			address.position.as_range.start = 1;
			address.position.as_range.end = context->buffer.count;
			c += 1;
			result = ED_ADDRESS_RANGE;
			goto defer;
		} else {
			address.position.as_line = context->line;
			result = ED_ADDRESS_LINE;
			goto defer;
		}
	}

	if (*c != ',') {
		address.position.as_line = start;
		result = ED_ADDRESS_LINE;
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
		address.position.as_line = start;
		result = ED_ADDRESS_LINE;
		goto defer;
	}

	address.position.as_range.start = start;
	address.position.as_range.end = end;
	result = ED_ADDRESS_RANGE;
	goto defer;

defer:
	*line = c;
	address.type = result;
	return address;
}

// Parse a command type from user input.
//
// `line` is updated for commands where it needs to be.
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
	case 'm':
		*line += 1;
		return ED_CMD_MOVE;
	case 'n':
		return ED_CMD_PRINT_NUM;
	case 'p':
		*line += 1;
		if (*line[0] == 'n')
			return ED_CMD_PRINT_NUM;
		return ED_CMD_PRINT;
	case 'P':
		return ED_CMD_TOGGLE_PROMPT;
	case 'q':
		return ED_CMD_QUIT;
	case 'Q':
		return ED_CMD_FORCE_QUIT;
	case 'u':
		return ED_CMD_UNDO;
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

// VALIDATION

// Checks that an address is valid within the bounds of the context's buffer.
bool address_out_of_range(Ed_Address address, bool allow_zero)
{
	Ed_Context *context = &ed_global_context;

	switch (address.type) {
	case ED_ADDRESS_LINE: {
		if (address.position.as_line == 0)
			return !allow_zero;
		size_t start = line_to_index(address.position.as_line);
		return !lb_contains(context->buffer, start);
	} break;
	case ED_ADDRESS_RANGE: {
		if (address.position.as_range.start == 0)
			return !allow_zero;
		size_t start = line_to_index(address.position.as_range.start);
		size_t end = line_to_index(address.position.as_range.end);
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

// COMMAND HANDLERS

bool ed_cmd_append(Ed_Address address)
{
	Ed_Context *context = &ed_global_context;
	if (address.type != ED_ADDRESS_LINE ||
	    address_out_of_range(address, true)) {
		ed_return_error(ED_ERROR_INVALID_ADDRESS);
	}

	Line_Builder lb = { 0 };
	ssize_t result = lb_read_to_dot(&lb);
	if (result < 0) {
		ed_return_error(ED_ERROR_UNKNOWN);
	}

	size_t amount = lb.count;
	ed_context_insert(&lb, address.position.as_line);
	context->line = address.position.as_line + amount;

	return true;
}

bool ed_cmd_change(Ed_Address address)
{
	Ed_Context *context = &ed_global_context;

	if (address_out_of_range(address, false)) {
		ed_return_error(ED_ERROR_INVALID_ADDRESS);
	}

	Line_Builder lb = { 0 };
	ssize_t result = lb_read_to_dot(&lb);
	if (result < 0) {
		ed_return_error(ED_ERROR_UNKNOWN);
	}

	if (address.type == ED_ADDRESS_LINE) {
		size_t start = line_to_index(address.position.as_line);

		lb_append(&context->yank_register,
			  strdup(context->buffer.items[start]));
		ed_context_overwrite(&lb, start, start);
	} else {
		if (context->yank_register.items != NULL) {
			lb_clear(context->yank_register);
		}

		size_t start = line_to_index(address.position.as_range.start);
		size_t end = line_to_index(address.position.as_range.end);

		for (size_t i = start; i <= end; ++i) {
			lb_append(&context->yank_register,
				  strdup(context->buffer.items[i]));
		}

		ed_context_overwrite(&lb, start, end);
	}

	return true;
}

bool ed_cmd_delete(Ed_Address address)
{
	Ed_Context *context = &ed_global_context;

	if (address_out_of_range(address, false)) {
		ed_return_error(ED_ERROR_INVALID_ADDRESS);
	}

	if (address.type == ED_ADDRESS_LINE) {
		if (context->yank_register.items != NULL) {
			lb_clear(context->yank_register);
		}

		size_t start = line_to_index(address.position.as_line);

		lb_append(&context->yank_register,
			  strdup(context->buffer.items[start]));
		ed_context_pop(start, start);
	} else {
		if (context->yank_register.items != NULL) {
			lb_clear(context->yank_register);
		}

		size_t start = line_to_index(address.position.as_range.start);
		size_t end = address.position.as_range.end;
		for (size_t i = start; i < end; ++i) {
			lb_append(&context->yank_register,
				  strdup(context->buffer.items[i]));
		}

		ed_context_pop(start, end - 1);
	}

	return true;
}

bool ed_cmd_edit(char *line)
{
	Ed_Context *context = &ed_global_context;

	context->filename = strdup(line);

	FILE *f = fopen(line, "r");
	if (f == NULL) {
		context->change_count++;
		fprintf(stderr, "%s: No such file or directory\n", line);
		ed_return_error(ED_ERROR_INVALID_FILE);
	}

	ssize_t result = lb_read_file(&context->buffer, f);
	context->line = context->buffer.count > 0 ? context->buffer.count - 1 :
						    0;
	fclose(f);

	if (result < 0) {
		ed_return_error(ED_ERROR_UNKNOWN);
	}

	printf(PRISize "\n", result);

	return true;
}

bool ed_cmd_insert(Ed_Address address)
{
	Ed_Context *context = &ed_global_context;
	if (address.type != ED_ADDRESS_LINE ||
	    address_out_of_range(address, true)) {
		ed_return_error(ED_ERROR_INVALID_ADDRESS);
	}

	Line_Builder lb = { 0 };
	ssize_t result = lb_read_to_dot(&lb);
	if (result < 0) {
		ed_return_error(ED_ERROR_UNKNOWN);
	}

	context->line = address.position.as_line;
	ed_context_insert(&lb, line_to_index(context->line));
	free(lb.items);

	return true;
}

bool ed_cmd_join(Ed_Address address)
{
	Ed_Context *context = &ed_global_context;

	size_t start, end;
	if (address.type == ED_ADDRESS_LINE) {
		start = line_to_index(address.position.as_line);
		end = start + 1;
	} else {
		start = line_to_index(address.position.as_range.start);
		end = line_to_index(address.position.as_range.end);
	}

	if ((!lb_contains(context->buffer, start) && start != 0) ||
	    !lb_contains(context->buffer, end)) {
		ed_return_error(ED_ERROR_INVALID_ADDRESS);
	}

	for (size_t i = start + 1; i <= end; ++i) {
		int len = strlen(context->buffer.items[start]);
		context->buffer.items[start][len - 1] = '\0';

		char *result = strappend(context->buffer.items[start],
					 context->buffer.items[i]);
		free(context->buffer.items[start]);
		context->buffer.items[start] = result;
	}

	ed_context_pop(start + 1, end);

	return true;
}

bool ed_cmd_move(char *line, Ed_Address address)
{
	Ed_Context *context = &ed_global_context;

	Ed_Address target = ed_parse_address(&line);
	if (target.type != ED_ADDRESS_LINE ||
	    address_out_of_range(address, false) ||
	    address_out_of_range(target, true)) {
		ed_return_error(ED_ERROR_INVALID_ADDRESS);
	}

	Line_Builder lb = { 0 };
	if (address.type == ED_ADDRESS_LINE) {
		size_t start = line_to_index(address.position.as_line);

		lb_append(&lb, strdup(context->buffer.items[start]));
		ed_context_pop(start, start);
	} else {
		size_t start = line_to_index(address.position.as_range.start);
		size_t end = address.position.as_range.end;
		for (size_t i = start; i < end; ++i) {
			lb_append(&lb, strdup(context->buffer.items[i]));
		}

		ed_context_pop(start, end - 1);
	}

	ed_context_insert(&lb, target.position.as_line);
	return true;
}

bool ed_cmd_print(Ed_Address address)
{
	Ed_Context *context = &ed_global_context;

	if (address_out_of_range(address, false)) {
		ed_return_error(ED_ERROR_INVALID_ADDRESS);
	}

	if (address.type == ED_ADDRESS_LINE) {
		size_t start = line_to_index(address.position.as_line);
		printf("%s", context->buffer.items[start]);
	} else {
		lb_print(context->buffer, address.position.as_range.start,
			 address.position.as_range.end);
	}

	return true;
}

bool ed_cmd_print_num(Ed_Address address)
{
	Ed_Context *context = &ed_global_context;

	if (address_out_of_range(address, false)) {
		ed_return_error(ED_ERROR_INVALID_ADDRESS);
	}

	if (address.type == ED_ADDRESS_LINE) {
		size_t start = line_to_index(address.position.as_line);
		printf(PRISize "\t%s", address.position.as_line,
		       context->buffer.items[start]);
	} else {
		lb_printn(context->buffer, address.position.as_range.start,
			  address.position.as_range.end);
	}

	return true;
}

bool ed_cmd_put(Ed_Address address)
{
	Ed_Context *context = &ed_global_context;

	if (address_out_of_range(address, true)) {
		ed_return_error(ED_ERROR_INVALID_ADDRESS);
	}

	Line_Builder tmp = { 0 };
	lb_foreach(line, context->yank_register)
	{
		lb_append(&tmp, strdup(*line));
	}

	ed_context_insert(&tmp, address.type == ED_ADDRESS_LINE ?
					address.position.as_line :
					address.position.as_range.end);
	free(tmp.items);

	return true;
}

bool ed_cmd_undo()
{
	Ed_Context *context = &ed_global_context;
	if (context->change_count == context->back_changes) {
		ed_return_error(ED_ERROR_NO_UNDO);
	}
	lb_swap(&context->buffer, &context->back_buf);
	size_t tmp = context->change_count;
	context->change_count = context->back_changes;
	context->back_changes = tmp;
	return true;
}

bool ed_cmd_write(char *line)
{
	Ed_Context *context = &ed_global_context;

	if (strlen(line) != 0) {
		free(context->filename);
		context->filename = strdup(line);
	}

	if (context->filename == NULL || strlen(context->filename) == 0) {
		ed_return_error(ED_ERROR_INVALID_COMMAND);
	}

	FILE *f = fopen(context->filename, "w");
	if (f == NULL) {
		ed_return_error(ED_ERROR_INVALID_FILE);
	}

	lb_write_to_stream(context->buffer, f);
	fclose(f);

	return true;
}

bool ed_cmd_quit(bool *quit, bool force)
{
	Ed_Context *context = &ed_global_context;

	if (!force && context->change_count > 0) {
		context->change_count = 0;
		ed_return_error(ED_ERROR_UNSAVED_CHANGES);
	}
	*quit = true;
	return true;
}

// API

bool ed_handle_cmd(char *line, bool *quit)
{
	Ed_Context *context = &ed_global_context;

	Ed_Address address = ed_parse_address(&line);

	Ed_Cmd_Type cmd_type = ed_parse_cmd_type(&line);
	switch (cmd_type) {
	case ED_CMD_APPEND: {
		return ed_cmd_append(address);
	} break;
	case ED_CMD_CHANGE: {
		return ed_cmd_change(address);
	} break;
	case ED_CMD_DELETE: {
		return ed_cmd_delete(address);
	} break;
	case ED_CMD_EDIT: {
		return ed_cmd_edit(line);
	} break;
	case ED_CMD_FORCE_QUIT: {
		return ed_cmd_quit(quit, true);
	} break;
	case ED_CMD_INSERT: {
		return ed_cmd_insert(address);
	} break;
	case ED_CMD_JOIN: {
		return ed_cmd_join(address);
	} break;
	case ED_CMD_LAST_ERR: {
		ed_print_error();
		return true;
	} break;
	case ED_CMD_MOVE: {
		return ed_cmd_move(line, address);
	} break;
	case ED_CMD_PRINT: {
		return ed_cmd_print(address);
	} break;
	case ED_CMD_PRINT_NUM: {
		return ed_cmd_print_num(address);
	} break;
	case ED_CMD_PUT: {
		return ed_cmd_put(address);
	} break;
	case ED_CMD_QUIT: {
		return ed_cmd_quit(quit, false);
	} break;
	case ED_CMD_TOGGLE_ERR: {
		context->should_print_error = !context->should_print_error;
		return true;
	} break;
	case ED_CMD_TOGGLE_PROMPT: {
		context->prompt = !context->prompt;
		return true;
	} break;
	case ED_CMD_UNDO: {
		return ed_cmd_undo();
	} break;
	case ED_CMD_WRITE: {
		return ed_cmd_write(line);
	} break;
	case ED_CMD_INVALID: {
		// TODO: handle this better
		if (address.type != ED_ADDRESS_LINE) {
			ed_return_error(ED_ERROR_INVALID_COMMAND);
		} else if (!lb_contains(
				   context->buffer,
				   line_to_index(address.position.as_line))) {
			ed_return_error(ED_ERROR_INVALID_ADDRESS);
		} else {
			context->line = address.position.as_line;
		}
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
	case ED_ERROR_NO_ERROR:
		break;
	case ED_ERROR_INVALID_ADDRESS: {
		fprintf(stderr, "Invalid address.\n");
	} break;
	case ED_ERROR_INVALID_COMMAND: {
		fprintf(stderr, "Invalid command.\n");
	} break;
	case ED_ERROR_INVALID_FILE: {
		fprintf(stderr, "Cannot open input file\n");
	} break;
	case ED_ERROR_NO_UNDO: {
		fprintf(stderr, "Nothing to undo.\n");
	} break;
	case ED_ERROR_UNSAVED_CHANGES: {
		fprintf(stderr, "Warning: buffer modified\n");
	} break;
	case ED_ERROR_UNKNOWN: {
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
