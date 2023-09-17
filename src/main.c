#include <assert.h>
#include <errno.h>
#include <string.h>

#include "./ed.h"

/* static Ed_Context ed_global_context = { .buffer = { 0 }, .line = 0 }; */

#define fprompt(message, ...) fprintf(stderr, "[PROMPT] " message, __VA_ARGS__)
#define prompt(message) fprintf(stderr, "[PROMPT] " message)

#define ferr(message, ...) fprintf(stderr, "[ERROR] " message "\n", __VA_ARGS__)
#define err(message) fprintf(stderr, "[ERROR] " message "\n")

int main(void)
{
	int result = 0;

	prompt("Enter a command: ");
	char *line = NULL;
	size_t nsize = 0;
	ssize_t nread = getline(&line, &nsize, stdin);
	if (nread < 0) {
		err("Please provide a command.");
		return 1;
	}

	char *copy = line;
	Ed_Location loc = {0};
	Ed_Location_Type kind = ed_parse_location(&copy, &loc);
	switch (kind) {
		case ED_LOCATION_START:
			printf("start: %zu\n", loc.as_start);
			printf("line: %s\n", copy);
			result = 0;
			goto defer;
		case ED_LOCATION_RANGE:
			printf("start: %zu\nend: %zu\n", loc.as_range.start, loc.as_range.end);
			printf("line: %s\n", copy);
			result = 0;
			goto defer;
		case ED_LOCATION_INVALID:
			err("Invalid location.");
			result = 1;
			goto defer;
	}

defer:
	if (line) free(line);
	return result;
}
