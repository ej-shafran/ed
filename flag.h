#ifndef _FLAG_H
#define _FLAG_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <inttypes.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>

bool *flag_bool_null(const char *name, bool default_, const char *description, ...);
#define flag_bool(name, default_, description, ...) flag_bool_null(name, default_, description, __VA_ARGS__, NULL)
uint64_t *flag_uint64_null(const char *name, uint64_t default_, const char *description, ...);
#define flag_uint64(name, default_, description, ...) flag_uint64_null(name, default_, description, __VA_ARGS__, NULL)
size_t *flag_size_null(const char *name, size_t default_, const char *description, ...);
#define flag_size(name, default_, description, ...) flag_size_null(name, default_, description, __VA_ARGS__, NULL)
char **flag_str_null(const char *name, char *default_, const char *description, ...);
#define flag_str(name, default_, description, ...) flag_str_null(name, default_, description, __VA_ARGS__, NULL)
bool flag_parse(int argc, char **argv);
void flag_print_options(FILE *stream);
void flag_print_error(FILE *stream);
int flag_rest_argc();
char **flag_rest_argv();

#ifdef FLAG_IMPLEMENTATION

typedef enum {
	FLAG_BOOL = 0,
	FLAG_UINT64,
	FLAG_SIZE,
	FLAG_STR,
	COUNT_FLAG_TYPES
} Flag_Type;

static_assert(COUNT_FLAG_TYPES == 4, "Exhaustive Flag_Type definition");

typedef enum {
	FLAG_NO_ERROR = 0,
	FLAG_ERROR_UNKNOWN,
	FLAG_ERROR_NO_VALUE,
	FLAG_ERROR_INVALID_NUMBER,
	FLAG_ERROR_INTEGER_OVERFLOW,
	FLAG_ERROR_INVALID_SIZE_SUFFIX,
	COUNT_FLAG_ERRORS,
} Flag_Error;

static_assert(COUNT_FLAG_ERRORS == 6, "Exhaustive Flag_Error definition");

typedef union {
	bool as_bool;
	uint64_t as_uint64;
	size_t as_size;
	char *as_str;
} Flag_Value;

#define ALIAS_CAP 256

typedef struct {
	const char *name;
    const char *aliases[ALIAS_CAP];
    size_t alias_count;
	const char *description;
	Flag_Type type;
	Flag_Value value;
	Flag_Value default_;
} Flag;

#define FLAGS_CAP 256

typedef struct {
	Flag flags[FLAGS_CAP];
	size_t flags_count;

	Flag_Error flag_error;
	char *flag_error_name;

	int rest_argc;
	char **rest_argv;
} Flag_Context;

static Flag_Context flag_global_context;

Flag *flag_new(Flag_Type type, const char *name, const char *description, va_list aliases)
{
	Flag_Context *context = &flag_global_context;

	assert(context->flags_count < FLAGS_CAP && "Out of capacity for flags");
	Flag *flag = &context->flags[context->flags_count++];
	// zero-initialize the flag
	memset(flag, 0, sizeof(*flag));
	flag->type = type;
	flag->name = name;
	flag->description = description;

    const char *alias = va_arg(aliases, const char *);
    while (alias != NULL) {
        assert(flag->alias_count < ALIAS_CAP && "Out of capacity for aliases");

        flag->aliases[flag->alias_count++] = alias;
        alias = va_arg(aliases, const char *);
    }

    va_end(aliases);

	return flag;
}

bool *flag_bool_null(const char *name, bool default_, const char *description, ...)
{
    va_list aliases;
    va_start(aliases, description);
	Flag *flag = flag_new(FLAG_BOOL, name, description, aliases);
	flag->value.as_bool = default_;
	flag->default_.as_bool = default_;
	return &flag->value.as_bool;
}

uint64_t *flag_uint64_null(const char *name, uint64_t default_,
		      const char *description, ...)
{
    va_list aliases;
    va_start(aliases, description);
	Flag *flag = flag_new(FLAG_UINT64, name, description, aliases);
	flag->value.as_uint64 = default_;
	flag->default_.as_uint64 = default_;
	return &flag->value.as_uint64;
}

size_t *flag_size_null(const char *name, size_t default_, const char *description, ...)
{
    va_list aliases;
    va_start(aliases, description);
	Flag *flag = flag_new(FLAG_SIZE, name, description, aliases);
	flag->value.as_size = default_;
	flag->default_.as_size = default_;
	return &flag->value.as_size;
}

char **flag_str_null(const char *name, char *default_, const char *description, ...)
{
    va_list aliases;
    va_start(aliases, description);
	Flag *flag = flag_new(FLAG_STR, name, description, aliases);
	flag->value.as_str = default_;
	flag->default_.as_str = default_;
	return &flag->value.as_str;
}

char *flag_shift_args(int *argc, char ***argv)
{
	assert(*argc > 0);
	char *result = **argv;
	*argv += 1;
	*argc -= 1;
	return result;
}

uint64_t parse_uint64_flag(const char *nptr, char **endptr, register int base)
{
	Flag_Context *context = &flag_global_context;

	const char *s = nptr;
	uint64_t acc;
	int c;
	uint64_t cutoff;
	int neg = 0, any, cutlim;

	do {
		c = *s++;
	} while (isspace(c));
	if (c == '-') {
		neg = 1;
		c = *s++;
	} else if (c == '+')
		c = *s++;
	if ((base == 0 || base == 16) && c == '0' && (*s == 'x' || *s == 'X')) {
		c = s[1];
		s += 2;
		base = 16;
	}
	if (base == 0)
		base = c == '0' ? 8 : 10;
	cutoff = (uint64_t)UINT64_MAX / (uint64_t)base;
	cutlim = (uint64_t)UINT64_MAX % (uint64_t)base;
	for (acc = 0, any = 0;; c = *s++) {
		if (isdigit(c))
			c -= '0';
		else if (isalpha(c))
			c -= isupper(c) ? 'A' - 10 : 'a' - 10;
		else
			break;
		if (c >= base)
			break;
		if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
			any = -1;
		else {
			any = 1;
			acc *= base;
			acc += c;
		}
	}
	if (any < 0) {
		acc = UINT64_MAX;
		context->flag_error = FLAG_ERROR_INTEGER_OVERFLOW;
	} else if (neg)
		acc = -acc;
	if (endptr != 0)
		*endptr = (char *)(any ? s - 1 : nptr);
	return (acc);
}

bool flag_parse(int argc, char **argv)
{
	Flag_Context *context = &flag_global_context;

	(void)flag_shift_args(&argc, &argv);

	while (argc > 0) {
		char *flag = flag_shift_args(&argc, &argv);

		if (*flag != '-') {
			context->rest_argc = argc + 1;
			context->rest_argv = argv - 1;
			return true;
		}

		if (strcmp(flag, "--") == 0) {
			context->rest_argc = argc;
			context->rest_argv = argv;
			return true;
		}

		// remove the dash
		flag++;

		bool found = false;

		for (size_t i = 0; i < context->flags_count; i++) {
			bool is_name = strcmp(context->flags[i].name, flag) == 0;
			bool is_alias = false;
			for (size_t j = 0; j < context->flags[i].alias_count; j++) {
				if (strcmp(context->flags[i].aliases[j], flag) == 0) {
					is_alias = true;
				}
			}

			if (is_name || is_alias) {
				static_assert(COUNT_FLAG_TYPES == 4,
					      "Exhaustive flag type parsing");
				switch (context->flags[i].type) {
				case FLAG_BOOL: {
					context->flags[i].value.as_bool = true;
				} break;
				case FLAG_UINT64: {
					if (argc == 0) {
						context->flag_error =
							FLAG_ERROR_NO_VALUE;
						context->flag_error_name = flag;
						return false;
					}
					char *arg =
						flag_shift_args(&argc, &argv);

					char *endptr;
					uint64_t result = parse_uint64_flag(
						arg, &endptr, 10);

					if (*endptr != '\0') {
						context->flag_error =
							FLAG_ERROR_INVALID_NUMBER;
						context->flag_error_name = flag;
						return false;
					}

					if (result == UINT64_MAX &&
					    context->flag_error ==
						    FLAG_ERROR_INTEGER_OVERFLOW) {
						context->flag_error_name = flag;
						return false;
					}

					context->flags[i].value.as_uint64 =
						result;
				} break;
				case FLAG_SIZE: {
					if (argc == 0) {
						context->flag_error =
							FLAG_ERROR_NO_VALUE;
						context->flag_error_name = flag;
						return false;
					}
					char *arg =
						flag_shift_args(&argc, &argv);

					char *endptr;
					uint64_t result = parse_uint64_flag(
						arg, &endptr, 10);

					if (strcmp(endptr, "K") == 0) {
						result *= 1024;
					} else if (strcmp(endptr, "M") == 0) {
						result *= 1024 * 1024;
					} else if (strcmp(endptr, "G") == 0) {
						result *= 1024 * 1024 * 1024;
					} else if (strcmp(endptr, "") != 0) {
						context->flag_error =
							FLAG_ERROR_INVALID_SIZE_SUFFIX;
						context->flag_error_name = flag;
						return false;
					}

					if (result == ULLONG_MAX &&
					    errno == ERANGE) {
						context->flag_error =
							FLAG_ERROR_INTEGER_OVERFLOW;
						context->flag_error_name = flag;
						return false;
					}

					context->flags[i].value.as_size =
						result;
				} break;
				case FLAG_STR: {
					if (argc == 0) {
						context->flag_error =
							FLAG_ERROR_NO_VALUE;
						context->flag_error_name = flag;
						return false;
					}
					char *arg =
						flag_shift_args(&argc, &argv);
					context->flags[i].value.as_str = arg;
				} break;
				case COUNT_FLAG_TYPES:
					assert(0 && "Unreachable");
					exit(1);
				}

				found = true;
			}
		}

		if (!found) {
			context->flag_error = FLAG_ERROR_UNKNOWN;
			context->flag_error_name = flag;
			return false;
		}
	}

	context->rest_argc = argc;
	context->rest_argv = argv;
	return true;
}

void flag_print_options(FILE *stream)
{
	Flag_Context *context = &flag_global_context;
	for (size_t i = 0; i < context->flags_count; i++) {
		Flag *flag = &context->flags[i];

		fprintf(stream, "    -%s", flag->name);
		for (size_t i = 0; i < flag->alias_count; i++) {
			fprintf(stream, ", -%s", flag->aliases[i]);
		}
		fprintf(stream, "\n");
		fprintf(stream, "        %s\n", flag->description);
		static_assert(COUNT_FLAG_TYPES == 4,
			      "Exhaustive flag type defaults printing");
		switch (flag->type) {
		case FLAG_BOOL:
			if (flag->default_.as_bool) {
				fprintf(stream, "        Default: true\n");
			}
			break;
		case FLAG_UINT64:
			fprintf(stream, "        Default: %" PRIu64 "\n",
				flag->default_.as_uint64);
			break;
		case FLAG_SIZE:
			fprintf(stream, "        Default: %zu\n",
				flag->default_.as_size);
			break;
		case FLAG_STR:
			fprintf(stream, "        Default: %s\n",
				flag->default_.as_str);
			break;
		case COUNT_FLAG_TYPES:
			assert(0 && "Unreachable");
			exit(1);
		}
	}
}

void flag_print_error(FILE *stream)
{
	Flag_Context *context = &flag_global_context;

	static_assert(COUNT_FLAG_ERRORS == 6, "Exhaustive flag error printing");

	fprintf(stream, "ERROR: ");
	switch (context->flag_error) {
	case FLAG_NO_ERROR:
		// NOTE: don't call flag_print_error() if flag_parse() didn't return false, okay? ._.
		fprintf(stream,
			"Operation Failed Successfully! "
			"Please tell the developer of this software that they don't know what they are doing! :)");
		break;
	case FLAG_ERROR_UNKNOWN:
		fprintf(stream, "-%s: unknown flag\n",
			context->flag_error_name);
		break;
	case FLAG_ERROR_NO_VALUE:
		fprintf(stream, "-%s: no value provided\n",
			context->flag_error_name);
		break;
	case FLAG_ERROR_INVALID_NUMBER:
		fprintf(stream, "-%s: invalid number\n",
			context->flag_error_name);
		break;
	case FLAG_ERROR_INTEGER_OVERFLOW:
		fprintf(stream, "-%s: integer overflow\n",
			context->flag_error_name);
		break;
	case FLAG_ERROR_INVALID_SIZE_SUFFIX:
		fprintf(stream, "-%s: invalid size suffix\n",
			context->flag_error_name);
		break;
	case COUNT_FLAG_ERRORS:
	default:
		assert(0 && "Unreachable");
		exit(1);
	}
}

int flag_rest_argc()
{
	return flag_global_context.rest_argc;
}

char **flag_rest_argv()
{
	return flag_global_context.rest_argv;
}

#endif // FLAG_IMPLEMENTATION

#endif // _FLAG_H
