#ifndef __ANTI_H
#define __ANTI_H

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>
#include "./da.h"

#define cmd_append(cmd, ...) cmd_append_null(cmd, __VA_ARGS__, NULL)

#define REBUILD_SELF(argc, argv)                                                      \
	do {                                                                          \
		const char *source_path = __FILE__;                                   \
		int source_time = get_time(source_path);                              \
                                                                                      \
		assert(argc >= 1);                                                    \
		const char *binary_path = argv[0];                                    \
		int binary_time = get_time(binary_path);                              \
                                                                                      \
		if (source_time > binary_time) {                                      \
			fprintf(stderr,                                               \
				"[REBUILD] Rebuilding source file.\n");               \
			Cmd build_cmd = { 0 };                                        \
			cmd_append(&build_cmd, "gcc");                                \
			cmd_append(&build_cmd, "-o", binary_path);                    \
			cmd_append(&build_cmd, source_path);                          \
			bool result = cmd_run_sync(build_cmd);                        \
			free(build_cmd.items);                                        \
			if (!result) {                                                \
				fprintf(stderr,                                       \
					"[ERROR] Failed to rebuild. There should be " \
					"diagnostics above.\n");                      \
				exit(1);                                              \
			}                                                             \
                                                                                      \
			Cmd binary_cmd = { 0 };                                       \
			for (int i = 0; i < argc; i++) {                              \
				cmd_append(&binary_cmd, argv[i]);                     \
			}                                                             \
			result = cmd_run_sync(binary_cmd);                            \
			free(binary_cmd.items);                                       \
			if (result) {                                                 \
				exit(0);                                              \
			} else {                                                      \
				exit(1);                                              \
			}                                                             \
		}                                                                     \
	} while (0);

#define INVALID_PID -1

typedef int Pid;
typedef da(const char *) Cmd;

void cmd_append_null(Cmd *cmd, ...);
void cmd_render(Cmd cmd, String_Builder *output);
Pid cmd_run_async(Cmd cmd);
bool proc_wait(Pid pid);
bool cmd_run_sync(Cmd cmd);
int get_time(const char *pathname);

#ifdef ANTI_IMPLEMENTATION

void cmd_append_null(Cmd *cmd, ...)
{
	va_list args;

	va_start(args, cmd);

	const char *arg = va_arg(args, const char *);
	while (arg != NULL) {
		da_append(cmd, arg);
		arg = va_arg(args, const char *);
	}

	va_end(args);
}

void cmd_render(Cmd cmd, String_Builder *output)
{
	da_foreach(const char *, item, cmd)
	{
		sb_append_cstr(output, *item);
		da_append(output, ' ');
	}
	sb_append_nul(output);
}

Pid cmd_run_async(Cmd cmd)
{
	// Print out the command
	String_Builder sb = { 0 };
	cmd_render(cmd, &sb);
	fprintf(stderr, "[CMD] %s\n", sb.items);
	free(sb.items);

	// Fork off a child process
	pid_t cpid = fork();
	if (cpid < 0) {
		fprintf(stderr, "[ERROR] Could not fork child process: %s\n",
			strerror(errno));
		return INVALID_PID;
	}

	// Execute child process
	if (cpid == 0) {
		if (execvp(cmd.items[0], (char *const *)cmd.items) < 0) {
			fprintf(stderr,
				"[ERROR] Could not execute child process: %s\n",
				strerror(errno));
			exit(1);
		}
		assert(0 && "Unreachable - execvp never returns non-negative");
	}

	return cpid;
}

bool proc_wait(Pid pid)
{
	while (true) {
		int wait_status = 0;
		if (waitpid(pid, &wait_status, 0) < 0) {
			fprintf(stderr,
				"[ERROR] Could not wait on command (pid: %d): %s",
				pid, strerror(errno));
			return false;
		}

		if (WIFEXITED(wait_status)) {
			int exit_code = WEXITSTATUS(wait_status);
			if (exit_code != 0) {
				fprintf(stderr,
					"[ERROR] Command exited with exit code %d\n",
					exit_code);
				return false;
			} else {
				return true;
			}
		}
	}
}

bool cmd_run_sync(Cmd cmd)
{
	Pid pid = cmd_run_async(cmd);
	if (pid == INVALID_PID)
		return false;
	return proc_wait(pid);
}

/* get_time: get the last edited time for a file. returns negative integer upon
 * failure to stat file. */
int get_time(const char *pathname)
{
	struct stat statbuf = { 0 };
	if (stat(pathname, &statbuf) < 0) {
		fprintf(stderr,
			"[ERROR] Could not stat file (at path %s): %s\n",
			pathname, strerror(errno));
		return -1;
	}

	return statbuf.st_mtime;
}

#endif // ANTI_IMPLEMENTATION

#endif // __ANTI_H
