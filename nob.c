#include <stdbool.h>
#define NOB_IMPLEMENTATION
#include "nob.h"

#define FLAG_IMPLEMENTATION
#include "flag.h"

void print_subcommands(FILE *stream)
{

    fprintf(stderr, "Subcommands:\n");
    fprintf(stderr, "    build\n");
    fprintf(stderr, "        Build the project\n");
    fprintf(stderr, "    run\n");
    fprintf(stderr, "        Run the resulting executable\n");
}

void main_usage(FILE* stream)
{
    fprintf(stderr, "Usage: ./nob <SUBCOMMAND> [OPTIONS]\n");
    fprintf(stderr, "\n");
    print_subcommands(stream);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
	flag_print_options(stream);
}

void run_usage(FILE *stream)
{
    fprintf(stderr, "Usage: ./nob run\n");
    /* fprintf(stderr, "\n"); */
    /* fprintf(stderr, "Options:\n"); */
	/* flag_print_options(stream); */
}

void build_usage(FILE *stream)
{
    fprintf(stderr, "Usage: ./nob build [OPTIONS]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
	flag_print_options(stream);
}

bool run()
{
	nob_log(NOB_INFO, "running `run` subcommand.");

	Nob_Cmd cmd = { 0 };

	nob_cmd_append(&cmd, "./build/main");

	bool result = nob_cmd_run_sync(cmd);
	nob_cmd_free(cmd);
	return result;
}

bool build()
{
	nob_log(NOB_INFO, "running `build` subcommand.");

	nob_mkdir_if_not_exists("./build");

	Nob_Cmd cmd = { 0 };

	// TODO: add Windows support?
	nob_cmd_append(&cmd, "gcc");
	nob_cmd_append(&cmd,"-ggdb");
	nob_cmd_append(&cmd, "-Wall", "-Wpedantic", "-Wextra", "-Werror");
	nob_cmd_append(&cmd, "-o", "./build/main");
	nob_cmd_append(&cmd, "./src/main.c", "./src/ed.c");
	bool result = nob_cmd_run_sync(cmd);
    nob_cmd_free(cmd);
    return result;
}

bool run_command(int argc, char **argv, bool *help)
{
    if (!flag_parse(argc, argv)) {
        run_usage(stderr);
        return false;
    }

    if (*help) {
        run_usage(stdout);
        return true;
    }

    return run();
}

bool build_command(int argc, char **argv, bool *help)
{
    bool *run_after = flag_bool("-run", false, "Run executable after building");
    flag_add_alias(run_after, "r");

    if (!flag_parse(argc, argv)) {
        build_usage(stderr);
        return false;
    }

    if (*help) {
        build_usage(stdout);
        return true;
    }

    if (!build()) return false;

    if (*run_after) return run();

	return true;
}

int main(int argc, char **argv)
{
	NOB_GO_REBUILD_URSELF(argc, argv);

	bool *help = flag_bool("-help", false, "Print this help and exit");
	flag_add_alias(help, "h");

	if (!flag_parse(argc, argv)) {
		main_usage(stderr);
        return 1;
	}

	int rest_argc = flag_rest_argc();
	char **rest_argv = flag_rest_argv();

	if (*help) {
		main_usage(stdout);
	} else if (rest_argc < 1) {
		main_usage(stderr);
		fprintf(stderr, "Error: Missing subcommand.\n");
        return 1;
	} else if (strcmp(rest_argv[0], "build") == 0) {
		if(!build_command(rest_argc, rest_argv, help)) return 1;
	} else if (strcmp(rest_argv[0], "run") == 0) {
		if(!run_command(rest_argc, rest_argv, help)) return 1;
    } else {
		main_usage(stderr);
		fprintf(stderr, "Error: Unrecognized subcommand.\n");
        return 1;
	}

	return 0;
}
