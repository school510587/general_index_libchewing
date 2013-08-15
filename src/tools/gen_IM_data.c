/**
 * gen_IM_data.c
 *
 * Copyright (c) 2013
 *      libchewing Core Team. See ChangeLog for details.
 *
 * See the file "COPYING" for information on usage and redistribution
 * of this file.
 */

/**
 * @file gen_IM_data.c
 *
 * @brief Generation of index tree for given non-zhuin IM.\n
 *
 *      This program reads in an IM definition cin file.\n
 *      Output a database file containing a key-in index tree.\n
 *      For details of structure, see init_database.c
 */

#include <dirent.h> /* For chdir(). */
#include <libgen.h> /* For basename(). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "build_tool.h"

#define CIN_EXTENSION ".cin"

/* Compare words by string only. */
int compare_word_by_str(const void *x, const void *y)
{
	return strcmp( ((const WordData*)x)->text.phrase, ((const WordData*)y)->text.phrase);
}

/**
 * Find all possibilities of keyin sequences using given range in word_data for
 * each word. Note that width refers to the n-th enumerated word. The recursion
 * stops when the end of phrase is reached, whose interval has the same value in
 * "from" and "to" fields.
 */
void find_keyin_sequence(const IntervalType range[], uint32_t keyin_buf[], int width)
{
	if( range[width].from < range[width].to) {
		int i;

		for(i = range[width].from; i < range[width].to; i++) {
			keyin_buf[width] = word_data[i].text.phone[0];
			find_keyin_sequence(range, keyin_buf, width+1);
		}
		keyin_buf[width] = 0;
	}
	/* A keyin sequence is determined when end of string is found. */
	else {
		if( width == 0 ) return;
		memcpy(phrase_data[num_phrase_data].phone, keyin_buf,
			sizeof(phrase_data[num_phrase_data].phone));
		++num_phrase_data;
	}
}

/**
 * The function enumerates all possibilities for a phrase. It fetches data of
 * each word.  Considering that there may be many keyin methods for a word,
 * IntervalType is used to handle this circumstance. The return value indicates
 * position of '\0', satisfying the least access to the mmap for dictionary.
 */
const char *enumerate_keyin_sequence(const char *phrase)
{
	IntervalType range[MAX_PHRASE_LEN + 1] = {0};
	uint32_t keyin_buf[MAX_PHRASE_LEN + 1] = {0};
	const char *p = phrase;
	WordData tmpWord, *q, *r;
	int old_num_phrase_data = num_phrase_data, i;

	/* Not handle null or length-one phrases. */
	if( *p == '\0' || phrase[ueBytesFromChar(*phrase)] == '\0' )
		return phrase + ueBytesFromChar(*phrase);

	/* Set up range of each word in word_data. */
	for(i = 0; *p; i++) {
		memcpy( tmpWord.text.phrase, p, ueBytesFromChar(*p) );
		q = (WordData*)bsearch(&tmpWord, word_data, num_word_data,
			sizeof(tmpWord), compare_word_by_str);
		if( q ) {
			for(r = q; r > word_data && !strcmp(r->text.phrase, (r-1)->text.phrase); r--);
			range[i].from = r-word_data;
			for(r = q+1; r < word_data+num_word_data && !strcmp(r->text.phrase, (r-1)->text.phrase); r++);
			range[i].to = r-word_data;
			p += ueBytesFromChar(*p);
		}
		else {
			fprintf(stderr, "Warning: `%s' cannot be input from cin.\n", phrase);
			while( *p ) p++;
			return p;
		}
	}

	/* Note: 0 means the first (0th) word is referenced. */
	find_keyin_sequence( range, keyin_buf, 0);

	/* FIXME: freq cannot be quickly obtained under the new structure. */
	for( i = old_num_phrase_data; i < num_phrase_data; i++) {
		strcpy(phrase_data[i].phrase, phrase);
		phrase_data[i].freq = 1;
	}

	return p;
}

int main(int argc, char *argv[])
{
	plat_mmap dict_map;
	long dict_size;
	size_t offset=0, csize;
	const char *dict, *p;
	int cin_path_id;

	for(cin_path_id = 1; cin_path_id < argc; cin_path_id++){
		size_t l = strlen( argv[cin_path_id] );
		if(l>4 && !strcmp( &argv[cin_path_id][l-4], CIN_EXTENSION) ) break;
	}

	if( cin_path_id >= argc) {
		fprintf(stderr, "Usage: %s <cin_filename>\n", argv[0]);
		exit(-1);
	}

	read_IM_cin( argv[cin_path_id], EncodeKeyin );

	/* Go to data/, where the exe is. */
	chdir(dirname( argv[0] ));

	plat_mmap_set_invalid(&dict_map);
	dict_size = plat_mmap_create(&dict_map, DICT_FILE, FLAG_ATTRIBUTE_READ);
	csize = dict_size;
	dict=(const char*)plat_mmap_set_view(&dict_map, &offset, &csize);

	if( !dict ) {
		fprintf(stderr, "%s: Error reading system dictionary.\n", argv[0]);
		exit(-1);
	}

	for(p = dict; p < dict+dict_size; p++)
		p = enumerate_keyin_sequence( p );

	plat_mmap_close(&dict_map);
	return 0;
}
