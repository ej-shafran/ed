#include <stdbool.h>
#define NOB_IMPLEMENTATION
#include "nob.h"

#define FLAG_IMPLEMENTATION
#include "flag.h"

void print_subcommands(FILE *stream)
{
	fprintf(stream, "Subcommands:\n");
	fprintf(stream, "    build\n");
	fprintf(stream, "        Build the project\n");
	fprintf(stream, "    run\n");
	fprintf(stream, "        Run the resulting executable\n");
	fprintf(stream, "    test\n");
	fprintf(stream, "        Test the project\n");
}

void main_usage(FILE *stream)
{
	fprintf(stream, "Usage: ./nob <SUBCOMMAND> [OPTIONS]\n");
	fprintf(stream, "\n");
	print_subcommands(stream);
	fprintf(stream, "\n");
	fprintf(stream, "Options:\n");
	flag_print_options(stream);
}

void test_usage(FILE *stream)
{
	fprintf(stream, "Usage: ./nob test [OPTIONS]\n");
	fprintf(stream, "\n");
	fprintf(stream, "Options:\n");
	flag_print_options(stream);
}

void run_usage(FILE *stream)
{
	fprintf(stream, "Usage: ./nob run\n");
	/* fprintf(stream, "\n"); */
	/* fprintf(stream, "Options:\n"); */
	/* flag_print_options(stream); */
}

void build_usage(FILE *stream)
{
	fprintf(stream, "Usage: ./nob build [OPTIONS]\n");
	fprintf(stream, "\n");
	fprintf(stream, "Options:\n");
	flag_print_options(stream);
}

bool test()
{
	nob_log(NOB_INFO, "running `test` subcommand.");

	Nob_Cmd cmd = { 0 };

	nob_cmd_append(&cmd, "bash");
	nob_cmd_append(&cmd, "./test.bash");

	bool result = nob_cmd_run_sync(cmd);
	nob_cmd_free(cmd);
	return result;
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

	nob_cmd_append(&cmd, "gcc");
	nob_cmd_append(&cmd, "-ggdb");
	nob_cmd_append(&cmd, "-Wall", "-Wpedantic", "-Wextra", "-Werror");
	nob_cmd_append(&cmd, "-o", "./build/main");
	nob_cmd_append(&cmd, "./src/main.c", "./src/ed.c", "./src/lb.c");
#ifdef _WIN32
	nob_cmd_append(&cmd, "./src/getline.c");
#endif // _WIN32
	bool result = nob_cmd_run_sync(cmd);
	nob_cmd_free(cmd);
	return result;
}

bool test_command(int argc, char **argv, bool *help)
{
	bool *without_build =
		flag_bool("-without-build", false,
			  "Run tests without rebuilding executable.");
	flag_add_alias(without_build, "w");

	if (!flag_parse(argc, argv)) {
		test_usage(stderr);
		return false;
	}

	if (*help) {
		test_usage(stdout);
		return true;
	}

	if (!(*without_build)) {
		if (!build())
			return false;
	}

	return test();
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
	bool *run_after =
		flag_bool("-run", false, "Run executable after building");
	bool *test_after =
		flag_bool("-test", false, "Run tests after building");
	flag_add_alias(run_after, "r");
	flag_add_alias(test_after, "t");

	if (!flag_parse(argc, argv)) {
		build_usage(stderr);
		return false;
	}

	if (*help) {
		build_usage(stdout);
		return true;
	}

	if (!build())
		return false;

	if (*test_after)
		return test();

	if (*run_after)
		return run();

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
		if (!build_command(rest_argc, rest_argv, help))
			return 1;
	} else if (strcmp(rest_argv[0], "run") == 0) {
		if (!run_command(rest_argc, rest_argv, help))
			return 1;
	} else if (strcmp(rest_argv[0], "test") == 0) {
		if (!test_command(rest_argc, rest_argv, help))
			return 1;
	} else {
		main_usage(stderr);
		fprintf(stderr, "Error: Unrecognized subcommand.\n");
		return 1;
	}

	return 0;
}
