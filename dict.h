/* dict.h: header for a dict-style data structure
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
#ifndef UU_DICT_H
#define UU_DICT_H

#define UU_DICT_THIRD_COUNT (65536)
#define UU_DICT_THIRD_MASK (0xffff)
#define UU_DICT_SECOND_COUNT (256)
#define UU_DICT_SECOND_MASK (0xff0000)
#define UU_DICT_FIRST_MASK (~(UU_DICT_SECOND_MASK | UU_DICT_THIRD_MASK))

/*
 * This is a basic implementation of something resembling a map. It should be
 * decently fast to look stuff up.
 *
 * Most of the numbers we deal with are about 0x1fff or so, so our primary
 * focus is <65536. As such, the struct contains a linked-list of arrays with
 * up to UU_DICT_THIRD_COUNT members per for the eight high bits of the low
 * sixteen bits of the number. Then, a dynamically-resized array is used for
 * the low eight bits.
 *
 * See uu_dict_find_location()'s implementation for details.
 */
struct uu_dict {
	struct uu_dict* next;
	unsigned long bits;
	void** inner[UU_DICT_SECOND_COUNT];
};

struct uu_dict*
uu_dict_new(void);
void
uu_dict_free(struct uu_dict*);

void
uu_dict_add(struct uu_dict* dict, unsigned long key, void* value);
void
uu_dict_remove(struct uu_dict* dict, unsigned long key);
void*
uu_dict_lookup(struct uu_dict* dict, unsigned long key);

void
uu_dict_for_each(struct uu_dict* dict,
		 void (*func)(void* elem, void* data),
		 void* data);
#endif
