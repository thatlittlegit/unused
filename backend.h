/* backend.h: generic backend header for libopcodes
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
#ifndef UU_BACKEND_H
#define UU_BACKEND_H

#include <bfd.h>

struct uu_dict;

void
uu_backend(bfd* bfd, struct bfd_section* section, struct uu_dict* dict);

#endif
