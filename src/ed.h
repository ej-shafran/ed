#ifndef ED_H_
#define ED_H_

#include <stdio.h>
#include <stdbool.h>
#include "../da.h"

// Parse a command from user input.
//
// Returns `false` upon failure, `true` upon success.
// Sets `cmd` to the parsed command.
bool ed_handle_cmd(char *line, bool *quit);

// Clean up the global context.
void ed_cleanup();

// Whether `H` mode is active.
bool ed_should_print_error();

// Print the last error that occured.
void ed_print_error();

// Get line after printint prompt.
ssize_t ed_getline(char **lineptr, size_t *n, FILE *stream);

#endif // ED_H_
