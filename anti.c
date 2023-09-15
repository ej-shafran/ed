#include <dirent.h>

#define ANTI_IMPLEMENTATION
#include "anti.h"
#define FLAG_IMPLEMENTATION
#include "flag.h"

void set_compiler(Cmd *cmd)
{
	cmd_append(cmd, "gcc");
}

void set_debug_info(Cmd *cmd)
{
	cmd_append(cmd, "-ggdb");
}

void set_warnings(Cmd *cmd)
{
	cmd_append(cmd, "-Wall", "-Wpedantic");
}

void ensure_dir(const char *dirpath)
{
	DIR *dir = opendir(dirpath);
	if (dir) {
		return;
	} else if (errno == ENOENT) {
		Cmd cmd = { 0 };

		cmd_append(&cmd, "mkdir", "-p", dirpath);

		bool result = cmd_run_sync(cmd);
		free(cmd.items);
		if (!result) {
			fprintf(stderr, "[ERROR] Fatal - `mkdir` failed: %s\n",
				strerror(errno));
			exit(1);
		}
	} else {
		fprintf(stderr, "[ERROR] Fatal - `opendir` failed: %s\n",
			strerror(errno));
		exit(1);
	}
}

void set_output(Cmd *cmd)
{
	ensure_dir("./build");

	cmd_append(cmd, "-o", "./build/main");
}

void set_input(Cmd *cmd)
{
	cmd_append(cmd, "./src/main.c");
}

void usage(FILE *stream)
{
	fprintf(stream, "USAGE: ./anti [OPTIONS]\n");
	flag_print_options(stream);
}

int main(int argc, char **argv)
{
	REBUILD_SELF(argc, argv);

	bool *help =
		flag_bool("-help", false, "Print this help message and exit.", "h");
	bool *run_after =
		flag_bool("-run", false, "After building, also run the output.", "r");

	if (!flag_parse(argc, argv)) {
		usage(stderr);
		return 1;
	}

	if (*help) {
		usage(stdout);
		return 0;
	}

	Cmd cmd = { 0 };

	set_compiler(&cmd);
	set_debug_info(&cmd);
	set_warnings(&cmd);
	set_output(&cmd);
	set_input(&cmd);

	bool result = cmd_run_sync(cmd);
	free(cmd.items);
	if (result) {
		if (*run_after) {
			Cmd cmd = { 0 };

			cmd_append(&cmd, "./build/main");

			bool result = cmd_run_sync(cmd);
			free(cmd.items);
			if (!result)
				return 1;
		}

		return 0;
	} else {
		return 1;
	}
}
