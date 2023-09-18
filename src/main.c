#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "./ed.h"

int main(void)
{
	char *line = NULL;
	size_t nsize = 0;

	bool quit = false;
	while (!quit) {
		line = NULL;
		nsize = 0;
		ssize_t nread = ed_getline(&line, &nsize, stdin);
		char *copy = line;
		if (nread < 0) {
			return 1;
		}
		bool success = ed_handle_cmd(line, &quit);
		free(copy);
		if (!success) {
			fprintf(stderr, "?\n");
			if (ed_should_print_error())
				ed_print_error();
		}
	}

	ed_cleanup();
	return 0;
}
