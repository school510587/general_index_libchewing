/**
 * char.c
 *
 * Copyright (c) 1999, 2000, 2001
 *	Lu-chuan Kung and Kang-pen Chen.
 *	All rights reserved.
 *
 * Copyright (c) 2004, 2005, 2006, 2008
 *	libchewing Core Team. See ChangeLog for details.
 *
 * See the file "COPYING" for information on usage and redistribution
 * of this file.
 */

/**
 * @file char.c
 * @brief word data file
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global-private.h"
#include "char-private.h"
#include "private.h"
#include "plat_mmap.h"

static int CompUint16( const uint16_t *pa, const uint16_t *pb )
{
	return ( (*pa) - (*pb) );
}

void TerminateChar( ChewingData *pgdata )
{
}

int InitChar( ChewingData *pgdata , const char * prefix )
{
	return 0;
}

static void Str2Word( ChewingData *pgdata, Word *wrd_ptr )
{
	unsigned char size;
	size = *(unsigned char *) pgdata->static_data.char_cur_pos;
	pgdata->static_data.char_cur_pos = (unsigned char*) pgdata->static_data.char_cur_pos + sizeof(unsigned char);
	memcpy( wrd_ptr->word, pgdata->static_data.char_cur_pos, size );
	pgdata->static_data.char_cur_pos = (unsigned char*) pgdata->static_data.char_cur_pos + size;
	wrd_ptr->word[ size ] = '\0';
}

int GetCharFirst( ChewingData *pgdata, Word *wrd_ptr, uint16_t phoneid )
{
	uint16_t *pinx;

	pinx = (uint16_t *) bsearch(
		&phoneid, pgdata->static_data.arrPhone, pgdata->static_data.phone_num,
		sizeof( uint16_t ), (CompFuncType) CompUint16 );
	if ( ! pinx )
		return 0;

	pgdata->static_data.char_cur_pos = (unsigned char*)pgdata->static_data.char_ + pgdata->static_data.char_begin[ pinx - pgdata->static_data.arrPhone ];
	pgdata->static_data.char_end_pos = pgdata->static_data.char_begin[ pinx - pgdata->static_data.arrPhone + 1 ];
	Str2Word( pgdata, wrd_ptr );
	return 1;
}

int GetCharNext( ChewingData *pgdata, Word *wrd_ptr )
{
	if ( (unsigned char*)pgdata->static_data.char_cur_pos >= (unsigned char*)pgdata->static_data.char_ + pgdata->static_data.char_end_pos )
		return 0;
	Str2Word( pgdata, wrd_ptr );
	return 1;
}
