#ifndef DA_H_
#define DA_H_

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define da(type)                                \
	struct {                                    \
		type *items;                            \
		size_t count;                           \
		size_t capacity;                        \
	}

#define DA_CAP_INIT 255

#define da_append(da, item)                                 \
	do {                                                    \
		if ((da)->count >= (da)->capacity) {                \
			(da)->capacity = (da)->capacity == 0 ?          \
                DA_CAP_INIT :                               \
                (da)->capacity * 2;                         \
			(da)->items = realloc((da)->items,              \
                                  (da)->capacity *          \
                                  sizeof(*(da)->items));    \
			assert((da)->items != NULL &&                   \
			       "Could not reallcoate memory");          \
		}                                                   \
		(da)->items[(da)->count++] = (item);                \
	} while (0);

#define da_append_many(da, new_items, amount)               \
	do {                                                    \
		if ((da)->count + amount > (da)->capacity) {        \
			if ((da)->capacity == 0) {                      \
				(da)->capacity = DA_CAP_INIT;               \
			}                                               \
			while ((da)->count + amount > (da)->capacity) { \
				(da)->capacity *= 2;                        \
			}                                               \
			(da)->items = realloc((da)->items,              \
                                  (da)->capacity *          \
                                  sizeof(*(da)->items));    \
			assert((da)->items != NULL &&                   \
			       "Could not reallocate memory");          \
		}                                                   \
		memcpy((da)->items + (da)->count, new_items,        \
		       amount * sizeof(*(da)->items));              \
		(da)->count += amount;                              \
	} while (0);

#define da_foreach(type, item, da)                                      \
	for (type *item = (da).items; item < (da).items + (da).count; item++)

#define sb_append_cstr(sb, cstr)                \
	do {                                        \
		const char *s = (cstr);                 \
		int n = strlen(s);                      \
		da_append_many(sb, s, n);               \
	} while (0);

#define sb_append_nul(sb) da_append(sb, '\0')

typedef da(char) String_Builder;

#endif // DA_H_
