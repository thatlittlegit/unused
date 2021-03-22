/* dict.c: implementation for a map-style data structure
 *
 * Copyright (C) 2021 thatlittlegit
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, under version 3 of the License,
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "dict.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct uu_dict*
uu_dict_new(void)
{
	struct uu_dict* dict = (struct uu_dict*)malloc(sizeof(struct uu_dict));
	dict->next = NULL;
	dict->bits = 0;
	memset(dict->inner, 0, UU_DICT_SECOND_COUNT * sizeof(void*));

	return dict;
}

void
uu_dict_free(struct uu_dict* dict)
{
	struct uu_dict* current_dict;
	struct uu_dict* backup;

	current_dict = dict;
	while (current_dict) {
		unsigned i;

		for (i = 0; i < UU_DICT_SECOND_COUNT; i++)
			free(current_dict->inner[i]);

		backup = current_dict->next;
		free(current_dict);
		current_dict = backup;
	}
}

static void***
uu_dict_find_location(struct uu_dict* dict, unsigned long key)
{
	struct uu_dict* current_dict;

	current_dict = dict;
	while (current_dict && current_dict->bits != (key & UU_DICT_FIRST_MASK))
		current_dict = current_dict->next;

	if (current_dict == NULL)
		return NULL;

	return &current_dict->inner[(key & UU_DICT_SECOND_MASK) >> 16];
}

void
uu_dict_add(struct uu_dict* dict, unsigned long key, void* value)
{
	void*** location;
	struct uu_dict* old_next;

	location = uu_dict_find_location(dict, key);

	if (location == NULL) {
		old_next = dict->next;
		dict->next = (struct uu_dict*)malloc(sizeof(struct uu_dict));
		dict->next->next = old_next;
		dict->next->bits = key & UU_DICT_FIRST_MASK;
		memset(dict->next->inner, 0,
		       UU_DICT_SECOND_COUNT * sizeof(void*));
		location = uu_dict_find_location(dict->next, key);
	}

	if (*location == NULL) {
		*location = calloc(UU_DICT_THIRD_COUNT, sizeof(void*));
		memset(*location, 0, UU_DICT_THIRD_COUNT * sizeof(void*));
	}

	(*location)[key & UU_DICT_THIRD_MASK] = value;
}

void
uu_dict_remove(struct uu_dict* dict, unsigned long key)
{
	void*** location;

	location = uu_dict_find_location(dict, key);

	if (location == NULL)
		return;

	if (*location == NULL)
		return;

	(*location)[key & UU_DICT_THIRD_MASK] = NULL;
}

void*
uu_dict_lookup(struct uu_dict* dict, unsigned long key)
{
	void*** location = uu_dict_find_location(dict, key);

	if (location == NULL)
		return NULL;

	if (*location == NULL)
		return NULL;

	return (*location)[key & UU_DICT_THIRD_MASK];
}

void
uu_dict_for_each(struct uu_dict* dict,
		 void (*func)(void* elem, void* data),
		 void* data)
{
	struct uu_dict* current_dict;

	current_dict = dict;
	while (current_dict) {
		unsigned i;

		for (i = 0; i < UU_DICT_SECOND_COUNT; i++) {
			unsigned j;

			if (current_dict->inner[i] == NULL)
				continue;

			for (j = 0; j < UU_DICT_THIRD_COUNT; j++) {
				if (current_dict->inner[i][j])
					func(current_dict->inner[i][j], data);
			}
		}

		current_dict = current_dict->next;
	}
}
