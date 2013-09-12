/**
 * init_database.c
 *
 * Copyright (c) 2013
 *      libchewing Core Team. See ChangeLog for details.
 *
 * See the file "COPYING" for information on usage and redistribution
 * of this file.
 */

/**
 * @file init_database.c
 *
 * @brief Initialization of system dictionary and phone phrase tree.\n
 *
 *	This program reads in source of dictionary.\n
 *	Output a database file containing a phone phrase tree, and a dictionary file\n
 * containing non-duplicate phrases. To determine frequency of each phrase in\n
 * generation of other IM index, it outputs a log of 32-bit binary integers recording\n
 * total frequency for each non-duplicate phrase for build-time requirement of other\n
 * IM index.\n
 *	Each node represents a single phone.\n
 *	The output file contains a random access array, where each record includes:\n
 *	\code{
 *	       uint32_t key; may be phone data or record of input keys
 *	       int32_t child.begin, child.end; for internal nodes (key != 0)
 *	       uint32_t phrase.pos; for leaf nodes (key == 0), position of phrase in dictionary
 *	       int32_t phrase.freq; for leaf nodes (key == 0), frequency of the phrase
 *	}\endcode
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "build_tool.h"
#include "chewing-utf8-util.h"
#include "global-private.h"
#include "key2pho-private.h"

/* For ALC macro */
#include "private.h"

#define CHARDEF		 "%chardef"
#define BEGIN		   "begin"
#define END		     "end"
#define MAX_LINE_LEN	    (1024)
#define MAX_WORD_DATA	   (60000)
#define MAX_PHRASE_BUF_LEN      (149)
#define MAX_PHRASE_DATA	 (420000)

const char USAGE[] =
	"Usage: %s <phone.cin> <tsi.src>\n"
	"This program creates the following new files:\n"
	"* " PHONE_TREE_FILE "\n\tindex to phrase file (dictionary)\n"
	"* " DICT_FILE "\n\tmain phrase file\n"
#ifdef SUPPORT_MULTI_IM
	"* " FREQ_FILE "\n\tlog of total frequency\n"
#endif
;

/*
 * Please see TreeType for data field. pFirstChild points to the first of its
 * child list. pNextSibling points to its right sibling, where it and its right
 * sibling are both in the child list of its parent. However, pNextSibling will
 * become next-pointer like linked list, which makes writing of index-tree file
 * become a sequential traversal rather than BFS.
 */
typedef struct _tNODE {
	TreeType data;
	struct _tNODE *pFirstChild;
	struct _tNODE *pNextSibling;
} NODE;

NODE *root;

/* word_data is sorted reversely, for stack-like push operation. */
int compare_word_by_phone(const void *x, const void *y)
{
	const WordData *a = (const WordData *)x;
	const WordData *b = (const WordData *)y;

	if (a->text->phone[0] != b->text->phone[0])
		return b->text->phone[0] - a->text->phone[0];

	/* Compare original index for stable sort */
	return b->index - a->index;
}

void store_phrase(const char *line, int line_num)
{
	const char DELIM[] = " \t\n";
	char buf[MAX_LINE_LEN];
	char *phrase;
	char *freq;
	char *bopomofo;
	char bopomofo_buf[MAX_UTF8_SIZE * ZUIN_SIZE + 1];
	size_t phrase_len;
	WordData word; /* For check. */
	int i;

	strncpy(buf, line, sizeof(buf));
	strip(buf);
	if (strlen(buf) == 0)
		return;

	if (num_phrase_data >= top_phrase_data) {
		fprintf(stderr, "Need to increase MAX_PHRASE_DATA to process\n");
		exit(-1);
	}

	/* read phrase */
	phrase = strtok(buf, DELIM);
	if (!phrase) {
		fprintf(stderr, "Error reading line %d, `%s'\n", line_num, line);
		exit(-1);
	}
	strncpy(phrase_data[num_phrase_data].phrase, phrase, sizeof(phrase_data[0].phrase));

	/* read frequency */
	freq = strtok(NULL, DELIM);
	if (!freq) {
		fprintf(stderr, "Error reading line %d, `%s'\n", line_num, line);
		exit(-1);
	}

	errno = 0;
	phrase_data[num_phrase_data].freq = strtol(freq, 0, 0);
	if (errno) {
		fprintf(stderr, "Error reading frequency `%s' in line %d, `%s'\n", freq, line_num, line);
		exit(-1);
	}

	/* read bopomofo */
	for (bopomofo = strtok(NULL, DELIM), phrase_len = 0;
		bopomofo && phrase_len < MAX_PHRASE_LEN;
		bopomofo = strtok(NULL, DELIM), ++phrase_len) {

		phrase_data[num_phrase_data].phone[phrase_len] = UintFromPhone(bopomofo);
		if (phrase_data[num_phrase_data].phone[phrase_len] == 0) {
			fprintf(stderr, "Error reading bopomofo `%s' in line %d, `%s'\n", bopomofo, line_num, line);
			exit(-1);
		}
	}
	if (bopomofo) {
		fprintf(stderr, "Phrase `%s' too long in line %d\n", phrase, line_num);
	}

	/* check phrase length & bopomofo length */
	if ((size_t)ueStrLen(phrase_data[num_phrase_data].phrase) != phrase_len) {
		fprintf(stderr, "Phrase length and bopomofo length mismatch in line %d, `%s'\n", line_num, line);
		exit(-1);
	}

	/* Check that each word in phrase can be found in word list. */
	word.text = ALC(PhraseData, 1);
	assert(word.text);
	for (i = 0; i < phrase_len; ++i) {
		ueStrNCpy(word.text->phrase, ueStrSeek(phrase_data[num_phrase_data].phrase, i), 1, 1);
		word.text->phone[0] = phrase_data[num_phrase_data].phone[i];
		if (bsearch(&word, word_data, num_word_data, sizeof(word), compare_word_by_text) == NULL) {
		        /* Please delete this #if after resoving exception phrase cases. */
#if 0
			PhoneFromUint(bopomofo_buf, sizeof(bopomofo_buf), word.text->phone[0]);
			fprintf(stderr, "Phrase:%d:error:`%s': `%s' has no phone `%s'\n", line_num, phrase_data[num_phrase_data].phrase, word.text->phrase, bopomofo_buf);
			exit(-1);
#endif
		}
	}
	free(word.text);

	if (phrase_len >= 2)
		++num_phrase_data;
}

int compare_phrase(const void *x, const void *y)
{
	const PhraseData *a = (const PhraseData*)x;
	const PhraseData *b = (const PhraseData*)y;
	int cmp = strcmp(a->phrase, b->phrase);

	/* If phrases are different, it returns the result of strcmp(); else it
	 * reports an error when the same phone sequence is found.
	 */
	if (cmp) return cmp;
	if (!memcmp(a->phone, b->phone, sizeof(a->phone))) {
		fprintf(stderr, "Duplicated phrase `%s' found.\n", a->phrase);
		exit(-1);
	}
		return b->freq - a->freq;
}

void read_tsi_src(const char *filename)
{
	FILE *tsi_src;
	char buf[MAX_LINE_LEN];
	int line_num = 0;

	tsi_src = fopen(filename, "r");
	if (!tsi_src) {
		fprintf(stderr, "Error opening the file %s\n", filename);
		exit(-1);
	}

	while (fgets(buf, sizeof(buf), tsi_src)) {
		++line_num;
		store_phrase(buf, line_num);
	}

	qsort(phrase_data, num_phrase_data, sizeof(phrase_data[0]), compare_phrase);
}

void write_phrase_data()
{
	FILE *dict_file;
#ifdef SUPPORT_MULTI_IM
	FILE *freq_file;
	int32_t total_freq = 0;
#endif
	PhraseData *cur_phr;
	PhraseData *last_phr = NULL;
	int i;
	int j;

	dict_file = fopen(DICT_FILE, "wb");
	if (!dict_file) {
		fprintf(stderr, "Cannot open output file.\n");
		exit(-1);
	}

#ifdef SUPPORT_MULTI_IM
	freq_file = fopen(FREQ_FILE, "wb");
	if (!freq_file) {
		fprintf(stderr, "Cannot open output file.\n");
		exit(-1);
	}
#endif

	/*
	 * Duplicate Chinese strings with common pronunciation are detected and
	 * not written into system dictionary. Written phrases are separated by
	 * '\0', for convenience of mmap usage.
	 * Note: word_data and phrase_data have been qsorted by strcmp in
	 *       reading.
	 */
	for (i = j = 0; i < num_word_data || j < num_phrase_data; last_phr = cur_phr) {
		if (i == num_word_data)
			cur_phr = &phrase_data[j++];
		else if (j == num_phrase_data)
			cur_phr = word_data[i++].text;
		else if (strcmp(word_data[i].text->phrase, phrase_data[j].phrase) < 0)
			cur_phr = word_data[i++].text;
		else
			cur_phr = &phrase_data[j++];

		if (last_phr && !strcmp(cur_phr->phrase, last_phr->phrase)) {
			cur_phr->pos = last_phr->pos;
#ifdef SUPPORT_MULTI_IM
			total_freq += cur_phr->freq;
#endif
		}
		else {
			cur_phr->pos = ftell(dict_file);
			fwrite(cur_phr->phrase, strlen(cur_phr->phrase) + 1, 1, dict_file);
#ifdef SUPPORT_MULTI_IM
			if (last_phr) {
				fwrite(&total_freq, 1, sizeof(total_freq), freq_file);
				total_freq = cur_phr->freq;
			}
#endif
		}
	}

#ifdef SUPPORT_MULTI_IM
	/* The last unwritten total_freq. */
	fwrite(&total_freq, 1, sizeof(total_freq), freq_file);
	fclose(freq_file);
#endif

	fclose(dict_file);
}

int main(int argc, char *argv[])
{
	if (argc != 3) {
		printf(USAGE, argv[0]);
		return -1;
	}

#ifdef SUPPORT_MULTI_IM
	read_IM_cin(argv[1], NULL);
#else
	read_IM_cin(argv[1]);
#endif
	read_tsi_src(argv[2]);
	write_phrase_data();
	write_index_tree(PHONE_TREE_FILE);
	return 0;
}
