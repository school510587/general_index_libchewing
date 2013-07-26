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
#if ! defined(USE_BINARY_DATA)
#include <assert.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global-private.h"
#include "char-private.h"
#include "private.h"
#include "plat_mmap.h"

#if ! defined(USE_BINARY_DATA)
static char *fgettab( char *buf, int maxlen, FILE *fp )
{
	int i;

	for ( i = 0; i < maxlen; i++ ) {
		buf[ i ] = (char) fgetc( fp );
		if ( feof( fp ) )
			break;
		if ( buf[ i ] == '\t' )
			break;
	}
	if ( feof( fp ) )
		return 0;
	buf[ i ] = '\0';
	return buf;
}
#endif

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
#ifndef USE_BINARY_DATA
	char buf[ 1000 ];
	uint16_t sh;

	fgettab( buf, 1000, pgdata->static_data.charfile );
	/* only read 6 bytes to wrd_ptr->word avoid buffer overflow */
	sscanf( buf, "%hu %6[^ ]", &sh, wrd_ptr->word );
	assert( wrd_ptr->word[0] != '\0' );
#else
	unsigned char size;
	size = *(unsigned char *) pgdata->static_data.char_cur_pos;
	pgdata->static_data.char_cur_pos = (unsigned char*) pgdata->static_data.char_cur_pos + sizeof(unsigned char);
	memcpy( wrd_ptr->word, pgdata->static_data.char_cur_pos, size );
	pgdata->static_data.char_cur_pos = (unsigned char*) pgdata->static_data.char_cur_pos + size;
	wrd_ptr->word[ size ] = '\0';
#endif
}

int GetCharFirst( ChewingData *pgdata, Word *wrd_ptr, uint16_t phoneid )
{
	uint16_t *pinx;

	pinx = (uint16_t *) bsearch(
		&phoneid, pgdata->static_data.arrPhone, pgdata->static_data.phone_num,
		sizeof( uint16_t ), (CompFuncType) CompUint16 );
	if ( ! pinx )
		return 0;

#ifndef USE_BINARY_DATA
	fseek( pgdata->static_data.charfile, pgdata->static_data.char_begin[ pinx - pgdata->static_data.arrPhone ], SEEK_SET );
#else
	pgdata->static_data.char_cur_pos = (unsigned char*)pgdata->static_data.char_ + pgdata->static_data.char_begin[ pinx - pgdata->static_data.arrPhone ];
#endif
	pgdata->static_data.char_end_pos = pgdata->static_data.char_begin[ pinx - pgdata->static_data.arrPhone + 1 ];
	Str2Word( pgdata, wrd_ptr );
	return 1;
}

int GetCharNext( ChewingData *pgdata, Word *wrd_ptr )
{
#ifndef USE_BINARY_DATA
	if ( ftell( pgdata->static_data.charfile ) >= pgdata->static_data.char_end_pos )
		return 0;
#else
	if ( (unsigned char*)pgdata->static_data.char_cur_pos >= (unsigned char*)pgdata->static_data.char_ + pgdata->static_data.char_end_pos )
		return 0;
#endif
	Str2Word( pgdata, wrd_ptr );
	return 1;
}
