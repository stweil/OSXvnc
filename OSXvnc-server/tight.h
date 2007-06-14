/*
 * tight.h - header file for Tight Encoding
 */

/*
 *  Copyright (C) 2000, 2001 Const Kaplinsky.  All Rights Reserved.
 *  Copyright (C) 1999 AT&T Laboratories Cambridge.  All Rights Reserved.
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

#ifndef __TIGHT_H__
#define __TIGHT_H__
#include <jpeglib.h>


/* Stuff dealing with palettes. */

typedef struct COLOR_LIST_s {
    struct COLOR_LIST_s *next;
    int idx;
    CARD32 rgb;
} COLOR_LIST;

typedef struct PALETTE_ENTRY_s {
    COLOR_LIST *listNode;
    int numPixels;
} PALETTE_ENTRY;

typedef struct PALETTE_s {
    PALETTE_ENTRY entry[256];
    COLOR_LIST *hash[256];
    COLOR_LIST list[256];
} PALETTE;
#endif
