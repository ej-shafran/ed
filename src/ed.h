#ifndef ED_H_
#define ED_H_

#include <stdio.h>
#include <stdbool.h>
#include "../da.h"

// LINE BUILDER

// Dynamic array of lines (nul-terminated with the '\n' at the end)
typedef da(char *) Ed_Line_Builder;

// Read lines from `stream` into `lb` until `condition` is met.
// Passing `""` as the condition will read until EOF.
bool ed_lb_read_from_stream(Ed_Line_Builder *lb, FILE *file, char *condition);

// Write all lines from `lb` into `stream`.
#define ed_lb_write_to_stream(lb, file) \
	da_foreach(line, lb)            \
	{                               \
		fputs(*line, file);     \
	}

// Insert the contents of `source` into `target` at index `address`,
// pushing off the contents of `target` to make room.
//
// `source` is emptied and cannot be used/freed after this call.
void ed_lb_insert_at(Ed_Line_Builder *target, Ed_Line_Builder *source,
		     size_t address);

// Insert the contents of `source` into `target` at index `address`,
// while removing the line at `address` from the target.
//
// `source` is emptied and cannot be used/freed after this call.
void ed_lb_pop_and_insert(Ed_Line_Builder *target, Ed_Line_Builder *source,
			  size_t address);

// Wrapper around `ed_lb_read_from_stream` that reads lines from STDIN
// until a line with just `"."` is encountered.
#define ed_lb_read_to_dot(lb) ed_lb_read_from_stream(lb, stdin, ".\n")

// Wrapper around `ed_lb_read_from_stream` that reads lines from `file` until EOF.
#define ed_lb_read_file(lb, file) ed_lb_read_from_stream(lb, file, "")

// Print lines from `lb` into `stream`
#define ed_lb_fprint(stream, lb, start, end)                  \
	do {                                                  \
		int i = 0;                                    \
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
#define ed_lb_fnum(stream, lb, start, end)                \
	do {                                              \
		int i = 0;                                \
		da_foreach(line, lb)                      \
		{                                         \
			i += 1;                           \
			if (i >= start && i <= end)       \
				fprintf(stream, "%d\n", i); \
		}                                         \
	} while (0);

// Wrapper around `ed_lb_fnum` which prints to STDOUT.
#define ed_lb_num(lb, start, end) ed_lb_fnum(stdout, lb, start, end)

// Print lines and their line numbers from `lb` into `stream`
#define ed_lb_fprintn(stream, lb, start, end)                           \
	do {                                                            \
		int i = 0;                                              \
		da_foreach(line, lb)                                    \
		{                                                       \
			i += 1;                                         \
			if (i >= start && i <= end)                     \
				fprintf(stream, "%d     %s", i, *line); \
		}                                                       \
	} while (0);

// Wrapper around `ed_lb_fprintn` which prints to STDOUT.
#define ed_lb_printn(lb, start, end) ed_lb_fprintn(stdout, lb, start, end)

// Free the allocated pointers in `lb`.
#define ed_lb_free(lb)               \
	do {                         \
		da_foreach(line, lb) \
		{                    \
			free(*line); \
		}                    \
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
	ED_CMD_EDIT,
	ED_CMD_WRITE,
	ED_CMD_NUM,
	ED_CMD_PRINT,
	ED_CMD_PRINT_NUM,
	ED_CMD_QUIT,
	ED_CMD_LAST_ERR,
	ED_CMD_TOGGLE_PROMPT,
	ED_CMD_TOGGLE_ERR,
	ED_CMD_INVALID
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

// Parse a command from user input.
//
// Returns `false` upon failure, `true` upon success.
// Sets `cmd` to the parsed command.
bool ed_handle_cmd(char *line, bool *quit);

// CONTEXT

// Clean up the global context.
void ed_cleanup();

// Whether `H` mode is active.
bool ed_should_print_error();

// Print the last error that occured.
void ed_print_error();

// Get line after printint prompt.
ssize_t ed_getline(char **lineptr, size_t *n, FILE *stream);

#endif // ED_H_
