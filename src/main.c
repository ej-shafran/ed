#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

#include "../da.h"

typedef da(char *) Line_Builder;

#define lb_free(lb)                  \
	da_foreach(char *, line, lb) \
	{                            \
		free(*line);         \
	}

#define lb_print(lb)                 \
	da_foreach(char *, line, lb) \
	{                            \
		printf("%s", *line); \
	}

bool read_to_dot(Line_Builder *lb)
{
	while (true) {
		char *line = NULL;
		size_t nsize = 0;
		ssize_t nread = getline(&line, &nsize, stdin);
		if (nread < 0) {
			free(line);
			return false;
		}

		if (strcmp(line, ".\n") == 0)
			break;
		da_append(lb, line);
	}

	return true;
}

int main(void)
{
	Line_Builder lb = { 0 };

	bool result = read_to_dot(&lb);

	if (result == false) {
		fprintf(stderr, "[ERROR] Fatal - `getline` failed: %s\n",
			strerror(errno));
		return 1;
	}

	lb_print(lb);
	lb_free(lb);
	return 0;
}
