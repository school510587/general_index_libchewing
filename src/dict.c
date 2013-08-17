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

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "global-private.h"
#include "plat_mmap.h"
#include "dict-private.h"
#include "memory-private.h"
#include "private.h"

typedef struct {
	plat_mmap mmap;
	const char *text;
} Dict_Instance;

/* Let system dictionary has only an instance. */
static Dict_Instance sys_dict;

void TerminateDict()
{
	if( ctx_count == 0) {
		plat_mmap_close( &sys_dict.mmap );
		sys_dict.text = NULL;
	}
}

int InitDict( const char *prefix )
{
	if( ctx_count == 0) {
		char filename[ PATH_MAX ];
		size_t len, offset, file_size, csize;

		len = snprintf( filename, sizeof( filename ), "%s" PLAT_SEPARATOR "%s", prefix, DICT_FILE );
		if ( len + 1 > sizeof( filename ) )
			return -1;

		plat_mmap_set_invalid( &sys_dict.mmap );
		file_size = plat_mmap_create( &sys_dict.mmap, filename, FLAG_ATTRIBUTE_READ );
		if ( file_size <= 0 )
			return -1;

		offset = 0;
		csize = file_size;
		sys_dict.text = (const char*)plat_mmap_set_view( &sys_dict.mmap, &offset, &csize );
		if ( !sys_dict.text )
			return -1;

		return 0;
	}
}

static int CompTreeType( const void *a, const void *b )
{
	return ( ((TreeType*)a)->key - ((TreeType*)b)->key );
}

/*
 * The function gets string of vocabulary from dictionary and its frequency from
 * tree index mmap, and stores them into buffer given by phr_ptr.
 */
static void GetVocabFromDict( ChewingData *pgdata, Phrase *phr_ptr )
{
	const TreeType *pLeaf = &pgdata->static_data.tree[ pgdata->static_data.tree_cur_pos ];
	strcpy(phr_ptr->phrase, sys_dict.text + pLeaf->phrase.pos);
	phr_ptr->freq = pLeaf->phrase.freq;
	pgdata->static_data.tree_cur_pos++;
}

int GetCharFirst( ChewingData *pgdata, Phrase *wrd_ptr, uint16_t key )
{
	const TreeType *pinx;
	TreeType keyNode = {0};
	keyNode.key = key;
	pinx = (const TreeType*) bsearch(
		&keyNode, pgdata->static_data.tree + pgdata->static_data.tree[0].child.begin,
		pgdata->static_data.tree[0].child.end - pgdata->static_data.tree[0].child.begin,
	sizeof( TreeType ), CompTreeType );
	if ( ! pinx )
		return 0;
	pgdata->static_data.tree_cur_pos = pinx->child.begin;
	pgdata->static_data.tree_end_pos = pinx->child.end;
	GetVocabFromDict( pgdata, wrd_ptr );
	return 1;
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
	GetVocabFromDict( pgdata, phr_ptr );
	return 1;
}

int GetVocabNext( ChewingData *pgdata, Phrase *phr_ptr )
{
	if ( pgdata->static_data.tree_cur_pos >= pgdata->static_data.tree_end_pos
		|| pgdata->static_data.tree[ pgdata->static_data.tree_cur_pos ].key != 0)
		return 0;
	GetVocabFromDict( pgdata, phr_ptr );
	return 1;
}
