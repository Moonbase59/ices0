/* in_flac.h
 * ices input plugin to read FLAC files as PCM
 *
 * Copyright (c) 2004 Brendan Cully <brendan@xiph.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * $Id: in_flac.h 7428 2004-07-31 09:11:47Z brendan $
 */

#ifndef IN_FLAC_H
#define IN_FLAC_H

#include "definitions.h"

int ices_flac_open(input_stream_t* self, char* buf, size_t len);

#endif
