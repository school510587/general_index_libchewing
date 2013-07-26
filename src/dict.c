/**
 * dict.c
 *
 * Copyright (c) 1999, 2000, 2001
 *	Lu-chuan Kung and Kang-pen Chen.
 *	All rights reserved.
 *
 * Copyright (c) 2004, 2005, 2008
 *	libchewing Core Team. See ChangeLog for details.
 *
 * See the file "COPYING" for information on usage and redistribution
 * of this file.
 */
#ifdef HAVE_CONFIG_H
  #include <config.h>
#endif

#if ! defined(USE_BINARY_DATA)
#include <stdlib.h>
#endif

#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "global-private.h"
#include "plat_mmap.h"
#include "dict-private.h"
#include "memory-private.h"

#if ! defined(USE_BINARY_DATA)
#include "private.h"
#endif

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

void TerminateDict( ChewingData *pgdata )
{
	plat_mmap_close( &pgdata->static_data.dict_mmap );
}

int InitDict( ChewingData *pgdata, const char *prefix )
{
	char filename[ PATH_MAX ];
	size_t len;
	size_t offset;
	size_t file_size;
	size_t csize;

	len = snprintf( filename, sizeof( filename ), "%s" PLAT_SEPARATOR "%s", prefix, DICT_FILE );
	if ( len + 1 > sizeof( filename ) )
		return -1;

	plat_mmap_set_invalid( &pgdata->static_data.dict_mmap );
	file_size = plat_mmap_create( &pgdata->static_data.dict_mmap, filename, FLAG_ATTRIBUTE_READ );
	if ( file_size <= 0 )
		return -1;

	offset = 0;
	csize = file_size;
	pgdata->static_data.dict = plat_mmap_set_view( &pgdata->static_data.dict_mmap, &offset, &csize );
	if ( !pgdata->static_data.dict )
		return -1;

	return 0;
}

/*
 * The function gets string of phrase from dictionary and its frequency from
 * tree index mmap, and stores them into buffer given by phr_ptr.
 */
static void GetPhraseFromDict( ChewingData *pgdata, Phrase *phr_ptr )
{
	const TreeType *pLeaf = &pgdata->static_data.tree[ pgdata->static_data.tree_cur_pos ];
	strcpy(phr_ptr->phrase, pgdata->static_data.dict + pLeaf->phrase.pos);
	phr_ptr->freq = pLeaf->phrase.freq;
	pgdata->static_data.tree_cur_pos++;
}

/*
 * Given an index of parent whose children are phrase leaves (phrase_parent_id),
 * the function initializes reading position (tree_cur_pos) and ending position
 * (tree_end_pos), and fetches the first phrase into phr_ptr.
 */
int GetPhraseFirst( ChewingData *pgdata, Phrase *phr_ptr, int phrase_parent_id )
{
	assert( ( 0 <= phrase_parent_id ) && ( phrase_parent_id * sizeof(TreeType) < pgdata->static_data.tree_size ) );

	pgdata->static_data.tree_cur_pos = pgdata->static_data.tree[ phrase_parent_id ].child.begin;
	pgdata->static_data.tree_end_pos = pgdata->static_data.tree[ phrase_parent_id ].child.end;
	GetPhraseFromDict( pgdata, phr_ptr );
	return 1;
}

int GetPhraseNext( ChewingData *pgdata, Phrase *phr_ptr )
{
#ifndef USE_BINARY_DATA
	if ( ftell( pgdata->static_data.dictfile ) >= pgdata->static_data.dict_end_pos )
		return 0;
#else
	if ( (unsigned char *)pgdata->static_data.dict_cur_pos >= (unsigned char *)pgdata->static_data.dict + pgdata->static_data.dict_end_pos )
		return 0;
#endif
	GetPhraseFromDict( pgdata, phr_ptr );
	return 1;
}
